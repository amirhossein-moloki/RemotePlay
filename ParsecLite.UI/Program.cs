using System;
using ParsecLite.UI.ViewModels;

namespace ParsecLite.UI
{
    class Program
    {
        static void Main(string[] args)
        {
            Console.WriteLine("========================================");
            Console.WriteLine("    Parsec-Lite Modern UI (Console Mock) ");
            Console.WriteLine("========================================");

            var viewModel = new MainViewModel();
            viewModel.PropertyChanged += (s, e) =>
            {
                if (e.PropertyName == nameof(MainViewModel.Status))
                    Console.WriteLine($"[UI State] Status: {viewModel.Status}");
                if (e.PropertyName == nameof(MainViewModel.Latency))
                    Console.WriteLine($"[Telemetry] Latency: {viewModel.Latency}ms");
            };

            Console.WriteLine("\n1. Simulate Start Hosting");
            viewModel.StartSession(true, "192.168.1.15");

            Console.WriteLine("\n2. Simulate Data Stream...");
            System.Threading.Thread.Sleep(500);

            Console.WriteLine("\n3. Simulate Stop Session");
            viewModel.StopSession();

            Console.WriteLine("\nDemo Complete.");
        }
    }
}
