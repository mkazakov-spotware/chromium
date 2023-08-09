using System;
using System.IO;
using System.Net.Http;
using System.Net.NetworkInformation;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

namespace testing_app
{
    static class Program
    {
        public static int DisplayNetworkConfiguration(IntPtr arg, int argLength)
        {
            WriteCommandLineArgs(arg);

            NetworkInterface[] adapters = NetworkInterface.GetAllNetworkInterfaces();
            foreach (NetworkInterface adapter in adapters)
            {
                IPInterfaceProperties properties = adapter.GetIPProperties();
                var path = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
                Console.WriteLine(path);
                Console.WriteLine(adapter.Description);
            }
            Console.WriteLine();
            return 2;
        }

        public static int HelloWorldFromDotNetCore(IntPtr arg, int argLength)
        {
            WriteCommandLineArgs(arg);

            Console.WriteLine("Hello World from .Net");
            Console.WriteLine();
            return 0;
        }

        public static void TlsRequest(string url)
        {
            using (HttpClient client = new HttpClient())
            {
                HttpResponseMessage response = client.GetAsync(url).Result;
                Console.WriteLine("IsSuccessStatusCode=", response.IsSuccessStatusCode);

                string responseBody = response.Content.ReadAsStringAsync().Result;
                if (response.IsSuccessStatusCode && responseBody is not null)
                {
                    Console.WriteLine("HTTPS request succeeded");
                }
                else
                {
                    Console.WriteLine("HTTPS request failed");
                }
            }
        }

        public static int ReverseLine(IntPtr arg, int argLength)
        {
            WriteCommandLineArgs(arg);

            bool windows = System.OperatingSystem.IsWindows();
            if (windows)
            {
                var hello = "Hello world from dotnet on Windows!!!";
                Console.Error.WriteLine(hello);

                var doc_folder = Environment.GetFolderPath(Environment.SpecialFolder.Personal);
                var out_path = Path.Combine(doc_folder, "out.txt");
                var out_file = new FileInfo(out_path);
                FileStream writable_stream2 = out_file.OpenWrite();
                var writer2 = new StreamWriter(writable_stream2, Encoding.ASCII);
                writer2.Write(hello);
                writer2.Flush();
                writable_stream2.Close();

        TlsRequest("https://www.google.com");
                return 0;
            }
            Console.WriteLine("Now please open a new shell and run pipes_test.sh");
            Console.WriteLine();

            Span<char> to_be_reversed = stackalloc char[2 << 12];
            var in_pipe = new FileInfo("in_pipe");
            var out_pipe = new FileInfo("out_pipe");

            while (true) {
                FileStream readable_stream = in_pipe.OpenRead();
                FileStream writable_stream = out_pipe.OpenWrite();
                var reader = new StreamReader(readable_stream, Encoding.ASCII);
                var writer = new StreamWriter(writable_stream, Encoding.ASCII);

                reader.Read(to_be_reversed);

                to_be_reversed.Reverse();

                writer.Write(to_be_reversed);
                writer.Flush();

                to_be_reversed.Clear();
                readable_stream.Close();
                writable_stream.Close();
            }
        }

        static void Main(string[] args)
        {
            var commandLineArgs = Environment.GetCommandLineArgs();

            Console.WriteLine($"[Target process] Started with {commandLineArgs.Length} command line arguments:");

            foreach (var arg in commandLineArgs)
                Console.WriteLine($"\t\"{arg}\"");
            
            Console.WriteLine("The display name is ");
            Console.WriteLine(typeof(Program).Assembly.FullName);

            Console.WriteLine("Qualified name is ");
            Console.WriteLine(typeof(Program).AssemblyQualifiedName);

            DisplayNetworkConfiguration(IntPtr.Zero, 0);

            Thread.Sleep(3000);
        }

        private static void WriteCommandLineArgs(IntPtr ptr)
        {
            if (ptr != IntPtr.Zero)
            {
                Console.Error.WriteLine("[Target process] 1. Started with command line arguments:");
                var ansi = Marshal.PtrToStringAnsi(ptr);
                Console.Error.WriteLine(ansi);
                return;
            }

            var commandLineArgs = Environment.GetCommandLineArgs();

            Console.Error.WriteLine($"[Target process] 2. Started with {commandLineArgs.Length} command line arguments:");

            foreach (var arg in commandLineArgs) Console.Error.WriteLine($"\t\"{arg}\"");
        }
    }
}
