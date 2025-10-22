using System;
using System.Diagnostics;
using System.IO;
using UnityEditor;
using UnityEngine;
using Application = UnityEngine.Application;
using Debug = UnityEngine.Debug;

namespace VAUnity
{
    /// <summary>
    /// Starts VA automatically when Unity starts, get called by VAUnity 
    /// </summary>
    [CreateAssetMenu(fileName = "VAStarter", menuName = "VAUnity/VAStarter", order = 1)]
    public class VAStarter : ScriptableObject
    {
        [Tooltip("Absolute VA directory")]
        [SerializeField] private string vaWorkingDirectory ="C:/Plugins//VA_win32-x64.vc12/";
        
        [Tooltip("Config File relative to the location of this Object.")]
        [SerializeField] private string vaPathConfig = "VACore.ini";
        
        /// <summary>
        /// relative path from working directory to config path
        /// </summary>
        private string _configPath;
        

        // [DllImport("user32.dll")]
        // static extern bool PostMessage(IntPtr hWnd, UInt32 Msg, int wParam, int lParam);
        private Process _process = null;
        private StreamWriter _messageStream;
        
        private bool _isInit = false;

        /// <summary>
        /// Starts VA Server with chosen Config file
        /// </summary>
        public void Init()
        {
            if (_isInit) return;
            GetConfigPath();
            StartProcess();
            _isInit = true;
        }

        /// <summary>
        /// Check if paths and files exists exists
        /// </summary>
        [ContextMenu("Check Paths")]
        public void CheckPaths()
        {
            if(!Directory.Exists(vaWorkingDirectory))
                Debug.LogWarning("Cannot find VA Dir " + vaWorkingDirectory);
            
            if(!GetConfigPath())
                Debug.LogWarning("Cannot find Config Path " + _configPath);
        }

        private void OnDisable()
        {
            _isInit = false;
        }

        /// <summary>
        /// Calculate the relative path of the config file
        /// </summary>
        private bool GetConfigPath()
        {
            _configPath = Path.GetDirectoryName(Application.dataPath + "/../" + AssetDatabase.GetAssetPath(this));
            
            var exists = File.Exists(_configPath +"/"+ vaPathConfig);
            Debug.Log(_configPath +"/"+ vaPathConfig);
            _configPath = GetRelativePath(_configPath, vaWorkingDirectory);
            return exists;
        }

        /// <summary>
        /// Returns a relative path from one path to another.
        /// </summary>
        /// <param name="folder">The destination path.</param>
        /// <param name="relativeTo">The source path the result should be relative to. This path is always considered to be a directory.</param>
        /// <returns></returns>
        private string GetRelativePath(string folder, string relativeTo)
        {
            var pathUri = new Uri(folder);
            // Folders must end in a slash
            if (!relativeTo.EndsWith(Path.DirectorySeparatorChar.ToString()))
            {
                relativeTo += Path.DirectorySeparatorChar;
            }
            var folderUri = new Uri(relativeTo);
            return Uri.UnescapeDataString(folderUri.MakeRelativeUri(pathUri).ToString().Replace('/', Path.DirectorySeparatorChar));
        }

        /// <summary>
        /// Start VA Server as a Windows Process
        /// </summary>
        /// <param name="redirectOutput">if false, VA Server Output would be redirected into Unity Console</param>
        private void StartProcess(bool redirectOutput=false)
        {
            try
            {
                if(!Directory.Exists(vaWorkingDirectory))
                    Debug.LogError("Cannot find VA Dir " + vaWorkingDirectory);
                    
                _process = new Process
                {
                    EnableRaisingEvents = false,
                    StartInfo =
                    {
                        WorkingDirectory = vaWorkingDirectory,
                        FileName = "cmd.exe",
                        // Arguments = "bin\\VAServer.exe localhost:12340 " + vaPathConfig,
                        Arguments = "/c" + "bin\\VAServer.exe localhost:12340 " + _configPath +"\\"+ vaPathConfig,
                        UseShellExecute = !redirectOutput,
                        // CreateNoWindow = true,
                        RedirectStandardOutput = redirectOutput,
                        // RedirectStandardInput = true,
                        RedirectStandardError = redirectOutput
                    }
                };
                if(redirectOutput)
                {
                    _process.OutputDataReceived += new DataReceivedEventHandler(DataReceived);
                    _process.ErrorDataReceived += new DataReceivedEventHandler(ErrorReceived);
                }
                _process.Start();
                if(redirectOutput)
                {
                    _process.BeginOutputReadLine();
                    _process.BeginErrorReadLine();
                }
                Debug.Log("Successfully launched app");
            }
            catch (Exception e)
            {
                Debug.LogError("Unable to launch app: " + e.Message);
            }
        }
        
        /// <summary>
        /// Convert Info Logs to Unity Log
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="eventArgs"></param>
        private void DataReceived(object sender, DataReceivedEventArgs eventArgs)
        {
            Debug.Log($"<b>[VA]</b> {eventArgs.Data}");
        }

        /// <summary>
        /// Convert Error Logs to Unity Error Log
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="eventArgs"></param>
        private void ErrorReceived(object sender, DataReceivedEventArgs eventArgs)
        {
            Debug.LogError($"<color=red><b>[VAError]</b> {eventArgs.Data}</color>");
        }

        /// <summary>
        /// tried to implement background Running from VA server, but didnt Worked
        /// </summary>
        public void OnApplicationQuit()
        {
            // const UInt32 WM_KEYDOWN = 0x0100;
            // const UInt32 WM_KEYUP = 0x0101;
            // const int VK_F5 = 0x74;
            // const int VK_Q = 0x51;
            //
            // PostMessage(process.MainWindowHandle, WM_KEYDOWN, VK_Q, 0);
            // PostMessage(process.MainWindowHandle, WM_KEYUP, VK_Q, 0);
            // SentInput("+");
            // SentInput("q");
            // process.WaitForExit();
            // if (process != null && !process.HasExited )
            // {
            //     process.Kill();
            // }
        }

        
    }
}