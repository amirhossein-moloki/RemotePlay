using System;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using ParsecLite.UI.Services;

namespace ParsecLite.UI.ViewModels
{
    public class MainViewModel : INotifyPropertyChanged
    {
        private string _status = "Ready";
        private float _latency;
        private float _fps;
        private bool _isSessionActive;

        public string Status
        {
            get => _status;
            set { _status = value; OnPropertyChanged(); }
        }

        public float Latency
        {
            get => _latency;
            set { _latency = value; OnPropertyChanged(); }
        }

        public float Fps
        {
            get => _fps;
            set { _fps = value; OnPropertyChanged(); }
        }

        public bool IsSessionActive
        {
            get => _isSessionActive;
            set { _isSessionActive = value; OnPropertyChanged(); }
        }

        public void StartSession(bool isHost, string ip)
        {
            var config = new ParsecConfig
            {
                IsHost = isHost,
                SelectedIp = ip,
                Bitrate = 8000,
                Fps = 60,
                UseHardwareEncoding = true
            };

            // ParsecCoreService.Parsec_StartSession(config); // Commented for mock
            IsSessionActive = true;
            Status = isHost ? "Hosting..." : "Connected to " + ip;

            // Mock telemetry update
            Latency = 15.2f;
            Fps = 60.0f;
        }

        public void StopSession()
        {
            // ParsecCoreService.Parsec_StopSession(); // Commented for mock
            IsSessionActive = false;
            Status = "Ready";
        }

        public event PropertyChangedEventHandler PropertyChanged;
        protected void OnPropertyChanged([CallerMemberName] string name = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
        }
    }
}
