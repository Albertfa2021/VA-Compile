using System;
using System.Runtime.InteropServices;
using System.Text;
using System.Collections.Generic;

namespace VA
{
    public class CSDummy
    {
        static void Main(string[] args)
        {
            string In = "Hallo Welt";
            Console.Write("String in: " + In);

            CSDummy W = new CSDummy();
            string Out = W.MyFunction(In);
            Console.WriteLine("String out: " + Out);

            if (In != Out)
                Console.WriteLine("String test failed.\n");
            else
                Console.WriteLine("String test OK.\n");

            MyDummyStruct StructIn = new MyDummyStruct();
            StructIn.bMyBool = false;
            StructIn.iMyInt = -1;
            StructIn.fMyFloat = 3.1415f;
            StructIn.dMyDouble = 99.0e33;
            Console.WriteLine("Struct in: " + StructIn.ToString());

            MyDummyStruct StructOut = W.MyStruct(StructIn);
            Console.WriteLine("Struct out: " + StructOut.ToString());

            if (!StructIn.Equals( StructOut ))
                Console.WriteLine("Struct test failed.\n");
            else
                Console.WriteLine("Struct test OK.\n");
            
            double[] SampleBuffer = { 1.0f, 1.0f, 1.0f, 1.0f };
            int iTransmittedSamples = NativeSetSampleBuffer(SampleBuffer, SampleBuffer.Length);
            
            int iResidualSamples = 1024;
            double[] SampleBufferIn = new double[iResidualSamples];
            iResidualSamples = NativeGetSampleBuffer(SampleBufferIn, SampleBufferIn.Length);
            while( iResidualSamples > 0 )
            {
                SampleBufferIn = new double[SampleBufferIn.Length + iResidualSamples]; // Potentially costly alloc 
                iResidualSamples = NativeGetSampleBuffer(SampleBufferIn, SampleBufferIn.Length);
            }


            List<int> liMyList = new List<int>();
            liMyList.Add(2);
            liMyList.Add(1);
            liMyList.Add(3);
            List<int> liMyNewList = W.MyIntVector(liMyList);

            if (!liMyNewList.Equals(liMyList))
                Console.WriteLine("Integer list test failed.\n");
            else
                Console.WriteLine("Integer list test OK.\n");

            MyDummyClass ClassIn = new MyDummyClass();
            ClassIn.iDataSize = 12;
            MyDummyClass ClassOut = W.MyClassIO(ClassIn);
            Console.WriteLine("Class In: " + ClassIn.ToString());
            Console.WriteLine("Class out: " + ClassOut.ToString());

            if (!ClassIn.Equals(ClassOut))
                Console.WriteLine("Class test failed.\n");
            else
                Console.WriteLine("Class test OK.\n");

            bool MyBool = true;
            int MyInt = -31;
            double MyDouble = 99.9f;
            List<byte[]> BufferList = new List<byte[]>();
            BufferList.Add(BitConverter.GetBytes(MyBool));
            BufferList.Add(BitConverter.GetBytes(MyInt));
            BufferList.Add(BitConverter.GetBytes(MyDouble));
            byte[] InBuf = BufferList[1];
            W.MyBufferIO(InBuf);

            Console.WriteLine("BufferList in C#: " + InBuf);

            byte[] OutBuf = new byte[5*4];
            System.Buffer.BlockCopy(BitConverter.GetBytes((int)(0)), 0, OutBuf, 0, 4);
            System.Buffer.BlockCopy(BitConverter.GetBytes((int)(2)), 0, OutBuf, 4, 4);
            /*
            System.Buffer.BlockCopy(BitConverter.GetBytes((int)(3)), 0, OutBuf, 8, 4);
            System.Buffer.BlockCopy(BitConverter.GetBytes((int)(4)), 0, OutBuf, 12, 4);
            System.Buffer.BlockCopy(BitConverter.GetBytes((int)(5)), 0, OutBuf, 16, 4);
             * */
            int iDiff = W.MyBufferIO(ref OutBuf);
            if (iDiff < 0)
                return;

            try
            {
                W.CauseException();
            }
            catch( Exception e )
            {
                Console.Write(e);
            }

            return;
        }


        // String in/out test

        public string MyFunction( string In )
        {
            StringBuilder sOut = new StringBuilder(In.Length);
            NativeMyFunction(In, sOut);
            return sOut.ToString();
        }

        [DllImport("VACSDummyWrapperD")]
        private static extern bool NativeMyFunction(string sIn, StringBuilder sOut);


        // Vector test
        public List<int> MyIntVector(List<int> liMyVector)
        {
            List<int> lOut = new List<int>();
            NativeMyIntVector(liMyVector, ref lOut);
            return lOut;
        }

        [DllImport("VACSDummyWrapperD")]
        private static extern bool NativeMyIntVector( List< int> lIn, ref List< int > lOut);


        //[DllImport("VACSDummyWrapperD")]
        //private static extern bool NativeMyIntVector([MarshalAs(UnmanagedType.LPArray, ArraySubType=UnmanagedType.I4) inval, [MarshalAs(UnmanagedType.LPArray, ArraySubType=UnmanagedType.I4));

        // Struct in/out test

        public struct MyDummyStruct
        {
            public bool bMyBool;
            public int iMyInt;
            public float fMyFloat;
            public double dMyDouble;

            public override string ToString()
            {
                string s = "Bool = " + bMyBool.ToString() +
                    ", Int = " + iMyInt.ToString() +
                    ", Float = " + fMyFloat.ToString() +
                    ", Double = " + dMyDouble.ToString();
                return s;
            }
        }

        public MyDummyStruct MyStruct(MyDummyStruct In)
        {
            MyDummyStruct Out = new MyDummyStruct();
            Out.iMyInt = -2;
            NativeMyStruct(In, ref Out);
            return Out;
        }

        [ StructLayout( LayoutKind.Sequential ) ]
        public struct MySample
        {
            public double time;
            public double amplitude;

            public MySample( double time, double amplitude )
            {
                this.time = time;
                this.amplitude = amplitude;
            }
        }

        [DllImport("VACSDummyWrapperD")]
        public static extern double NativeSampleBuffer([In, Out] MySample[] SampleBuffer, int size);

        [DllImport("VACSDummyWrapperD")]
        public static extern int NativeSetSampleBuffer( [In] double[] SampleBuffer, int NumSamples);
        [DllImport("VACSDummyWrapperD")]
        public static extern int NativeGetSampleBuffer( [Out] double[] SampleBuffer, int NumRequestedSamples);

        [DllImport("VACSDummyWrapperD")]
        private static extern bool NativeMyStruct(MyDummyStruct sIn, ref MyDummyStruct sOut);

        // Character buffer in/out test

        public void MyBufferIO(byte[] Buff)
        {
            if (Buff.Length == 0)
            {
                NativeGetBuffer(0, Buff);
            }
            else
            {
                int size = Buff.Length * Marshal.SizeOf(Buff[0]);
                int iRet = NativeGetBuffer(Buff.Length, Buff);
            }

        }

        [DllImport("VACSDummyWrapperD")]
        private static extern int NativeGetBuffer(int DataSize, byte[] Buffer);

        public int MyBufferIO(ref byte[] Buff)
        {
            if( Buff.Length == 0 )
                return 0;

            return NativeSetBuffer(Buff.Length, ref Buff);

        }

        [DllImport("VACSDummyWrapperD")]
        private static extern int NativeSetBuffer(int DataSize, ref byte[] Buffer);


        // Class in/out test

        public struct MyDummyClass
        {
            public int iDataSize;

            public override string ToString()
            {
                string s = "Data size = " + iDataSize.ToString();
                return s;
            }
        }

        public MyDummyClass MyClassIO(MyDummyClass In)
        {
            MyDummyClass Out = new MyDummyClass();
            Out.iDataSize = 0;
            bool bOK = NativeMyClassIO( In, ref Out );
            return Out;
        }

        [DllImport("VACSDummyWrapperD")] private static extern bool NativeMyClassIO( MyDummyClass In, ref MyDummyClass Out );

        public void CauseException()
        {
            NativeCauseException();
        }

        [DllImport("VACSDummyWrapperD")]
        private static extern void NativeCauseException();
    }
}
