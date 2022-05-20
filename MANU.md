# MANU branch of the google crashpad
We was missing "upload attach with crash dump" feature on Mac OS. That is why we created that fork.
# How to build
One will need to isntall depot_tools first.https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up
Then just run build_crashpad.py script. Note: the sript will create directory src and will clone code from repository to it and then build.
# How to test
0001-Add-attachment-handling.-Create-test-app-Mac-OS.patch will add simple test app to the crashpad source tree.
As far as test app is added to breakpad build system, the change - build - test dev circle is fast.
