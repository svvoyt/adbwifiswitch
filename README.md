# adbwifiswitch
Linux/C++ application to switch WiFi on Android device via Adb

   Application adbwifiswitch is a wrapper over Android debug bridge (adb). It is
collaborating with java application to switch WiFi on and off. In connect mode 
application runs adb as child process and starts intent

com.steinwurf.adbjoinwifi/.MainActivity via 'adb shell am start -n <activity> -e <extra>' command.
Intent has extras: 

mode [connect|disconnect]
uniq <uniq tag>
ssid SSID
password_type [WEP|WPA]
password PASSWORD



Build C++/Linux module:

$ cd $BuildDir
$ cmake <path to adbwifiswitch/adbwifiswitch dir>
$ make


Build java agent:

cd javaagent/adb-join-wifi
$ ./gradlew :app:assembleDebug

Install java agent:

$ adb install [-r] javaagent/adb-join-wifi/app/build/outputs/apk/debug/app-debug.apk


Java agent should be installed on the target device to allow switch WiFi on/off.
Android debug bridge should be available in PATH or '-a' switch could be used to specify 
adb executable location.

Connect to WiFi network:


./adbwifiswitch --ssid <SSID> --key <password>


Disconnect & disable WiFi on device:

./adbwifiswitch -d

Optional switches:
 -a|--adbcmd=<adb command>, default is "adb"
 -h|--help - usage hint
 -t|--type <WEP|WPA>, default is WPA
 -v|--verbose - noisy logging
