# VeSTige Verification Test for Asster
$ErrorActionPreference = "Stop"

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public class AssterVestigeTester {
    [StructLayout(LayoutKind.Sequential)]
    public struct AEffect {
        public int magic;
        public IntPtr dispatcher;
        public IntPtr process;
        public IntPtr setParameter;
        public IntPtr getParameter;
        public int numPrograms;
        public int numParams;
        public int numInputs;
        public int numOutputs;
        public int flags;
        public IntPtr resvd1;
        public IntPtr resvd2;
        public int initialDelay;
        public int realQualities;
        public int offQualities;
        public float ioRatio;
        public IntPtr obj;
        public IntPtr user;
        public int uniqueID;
        public int version;
        public IntPtr processReplacing;
        public IntPtr processDoubleReplacing;
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate IntPtr AudioMasterCallback(IntPtr effect, int opcode, int index, IntPtr val, IntPtr ptr, float opt);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate IntPtr DispatcherFunc(IntPtr effect, int opcode, int index, IntPtr val, IntPtr ptr, float opt);

    [DllImport(@"dist\Asster.dll", EntryPoint = "VSTPluginMain", CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr VSTPluginMain(AudioMasterCallback master);

    private static IntPtr DummyMaster(IntPtr effect, int opcode, int index, IntPtr val, IntPtr ptr, float opt) {
        return IntPtr.Zero;
    }

    public static void RunTest() {
        IntPtr pEffect = VSTPluginMain(DummyMaster);
        if (pEffect == IntPtr.Zero) throw new Exception("VSTPluginMain returned NULL!");
        AEffect fx = (AEffect)Marshal.PtrToStructure(pEffect, typeof(AEffect));
        if (fx.magic != 0x56737450) throw new Exception("Invalid magic: " + fx.magic);
        Console.WriteLine("[VeSTige] AEffect verified: Magic=0x" + fx.magic.ToString("X") + ", Programs=" + fx.numPrograms + ", Params=" + fx.numParams + ", Inputs=" + fx.numInputs + ", Outputs=" + fx.numOutputs);
        DispatcherFunc disp = (DispatcherFunc)Marshal.GetDelegateForFunctionPointer(fx.dispatcher, typeof(DispatcherFunc));
        IntPtr buf = Marshal.AllocHGlobal(64);
        disp(pEffect, 45, 0, IntPtr.Zero, buf, 0.0f); // effGetEffectName
        string name = Marshal.PtrToStringAnsi(buf);
        disp(pEffect, 47, 0, IntPtr.Zero, buf, 0.0f); // effGetVendorString
        string vendor = Marshal.PtrToStringAnsi(buf);
        Marshal.FreeHGlobal(buf);
        Console.WriteLine("[VeSTige] Plugin Name: '" + name + "', Vendor: '" + vendor + "'");
        disp(pEffect, 1, 0, IntPtr.Zero, IntPtr.Zero, 0.0f); // effClose
        Console.WriteLine("[VeSTige] Clean shutdown verified!");
    }
}
"@

[AssterVestigeTester]::RunTest()
Write-Host "ALL VESTIGE CHECKS PASSED!" -ForegroundColor Green
