"""Exercise production USB receiver registration with Android API stubs."""
from pathlib import Path
import tempfile,subprocess,os
root=Path(__file__).resolve().parents[2]
s=(root/'android/app/src/main/java/org/libsdl/app/HIDDeviceManager.java').read_text()
a=s.index('    private void registerUsbBroadcasts()');b=s.index('\n    UsbManager getUSBManager()',a)
fixture='''import java.util.*;
class Build {static class VERSION {static int SDK_INT;}}
class UsbManager {static final String ACTION_USB_DEVICE_ATTACHED="attach",ACTION_USB_DEVICE_DETACHED="detach";}
class IntentFilter {List<String> actions=new ArrayList<>();void addAction(String action){actions.add(action);}}
class Context {static final int RECEIVER_EXPORTED=2,RECEIVER_NOT_EXPORTED=4;List<IntentFilter> filters=new ArrayList<>();List<Integer> flags=new ArrayList<>();
 void registerReceiver(Object r,IntentFilter f){filters.add(f);flags.add(0);}void registerReceiver(Object r,IntentFilter f,int flag){filters.add(f);flags.add(flag);}}
class HIDDeviceManager {static final String ACTION_USB_PERMISSION="permission";Context mContext=new Context();Object mUsbBroadcast=new Object();
/* METHOD */
public static void main(String[] args){
 for(int api:new int[]{24,32,33,36}){Build.VERSION.SDK_INT=api;HIDDeviceManager d=new HIDDeviceManager();d.registerUsbBroadcasts();
 if(api<33){if(d.mContext.filters.size()!=1||d.mContext.filters.get(0).actions.size()!=3||d.mContext.flags.get(0)!=0)throw new AssertionError();}
 else {if(d.mContext.filters.size()!=2||d.mContext.flags.get(0)!=Context.RECEIVER_EXPORTED||d.mContext.flags.get(1)!=Context.RECEIVER_NOT_EXPORTED)throw new AssertionError();
 if(!d.mContext.filters.get(0).actions.equals(Arrays.asList("attach","detach"))||!d.mContext.filters.get(1).actions.equals(Arrays.asList("permission")))throw new AssertionError();}}
 System.out.println("PASS production USB registration: legacy compatibility, API33+ flags and private permission-reply isolation");
}}
'''
java=Path(os.environ['JAVA_HOME'])/'bin' if os.environ.get('JAVA_HOME') else None
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'HIDDeviceManager.java').write_text(fixture.replace('/* METHOD */',s[a:b]))
 subprocess.run([str(java/'javac') if java else 'javac','-d',d,str(p/'HIDDeviceManager.java')],check=True)
 subprocess.run([str(java/'java') if java else 'java','-cp',d,'HIDDeviceManager'],check=True)
