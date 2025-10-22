using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using VA;

namespace VA
{
    class VAExample
    {
        static void Main(string[] args)
        {
            VANet VAConnectionTest = new VANet();

            if(VAConnectionTest.Connect())
                Console.WriteLine("Succesfully connected to local VA server");

            if (VAConnectionTest.IsConnected())
                Console.WriteLine("Yes, connection really established");

            VAConnectionTest.Disconnect();

            if (!VAConnectionTest.IsConnected())
                Console.WriteLine("Yes, really disconnected");

            string HostIP = "localhost";
            int Port = 12340;
            VAConnectionTest.Connect(HostIP, Port);

            if (VAConnectionTest.IsConnected())
                Console.WriteLine("Succesfully connected to VA server at " + HostIP);
            else
                Console.WriteLine("Connection to '" + HostIP + "' failed");
            
            if (VAConnectionTest.IsConnected())
                Console.WriteLine("Yes, connection really established");

            try
            {
                VAConnectionTest.Reset();
            }
            catch( Exception e )
            {
                Console.Write(e);
            }

            VAConnectionTest.Disconnect();


            // From C# example

            VANet VAConnection = new VANet();
            VAConnection.Connect();
            VAConnection.Reset();

            string SignalSourceID = VAConnection.CreateSignalSourceBufferFromFile("$(DemoSound)");
            VAConnection.SetSignalSourceBufferPlaybackAction(SignalSourceID, "play");
            VAConnection.SetSignalSourceBufferLooping(SignalSourceID, true);

            int SoundSourceID = VAConnection.CreateSoundSource("C# example sound source");
            VAConnection.SetSoundSourcePose(SoundSourceID, new VAVec3(-2.0f, 1.7f, -2.0f), new VAQuat(0.0f, 0.0f, 0.0f, 1.0f));

            VAConnection.SetSoundSourceSignalSource(SoundSourceID, SignalSourceID);

            int HRIR = VAConnection.CreateDirectivityFromFile("$(DefaultHRIR)");

            int SoundReceiverID = VAConnection.CreateSoundReceiver("C# example sound receiver");
            VAConnection.SetSoundReceiverPose(SoundReceiverID, new VAVec3(0.0f, 1.7f, 0.0f), new VAQuat(0.0f, 0.0f, 0.0f, 1.0f));
            VAConnection.SetSoundReceiverDirectivity(SoundReceiverID, HRIR);

            try
            {
                double[] SampleBuffer = new double[3];
                SampleBuffer[2] = -1.0f;
                VAConnection.NativeUpdateGenericPath("MyGenericRenderer", SoundSourceID, SoundReceiverID, 1, 0.0, SampleBuffer.Length, SampleBuffer);
                VAConnection.NativeUpdateGenericPath("MyGenericRenderer", 2, -1, 1, 0.0, SampleBuffer.Length, SampleBuffer);
                VAConnection.NativeUpdateGenericPathFromFile("MyGenericRenderer", SoundSourceID, SoundSourceID, "stalbans_a_binaural.wav");
            }
            catch( Exception e )
            {
                Console.WriteLine("Could not update generic path renderer:" + e);
            }

            // do something that suspends the program ...

            VAConnection.Disconnect();
        }
    }
}
