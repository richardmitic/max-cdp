# MaxCDP
A MaxMSP frontend for the [Composers Desktop Project](http://www.unstablesound.net/cdp.html)

## Info
Based on the "threadpool" example object in the Max SDK.

### Building 
Clone the [Max SDK](https://github.com/Cycling74/max-sdk), then clone this repo in `source/advanced`. Build with Xcode. Make sure the `externals` directory is added to the Max search path.

### Using
CDP must be installed somewhere on your system. If it's not in the Max search path you can specify the root directory with the `@root` attribute.

Send messages to the object that look exactly like a CDP shell command, e.g. `distort fractal /path/to/sound.wav /path/to.sound.out.wav 3 0.5 -p0.5`

#### Using buffer~ objects
When sending messages to MaxCDP, any argument that starts with `__CDPIN` or `__CDPOUT` will be treated as the name of a Max buffer~. MaxCDP will save the contents of any `__CDPIN` buffers to your home directory, perform the CDP operation, and load any resulting audio file into a `__CDPOUT` buffer~.

E.g. `distort telescope __CDPIN_hello_buffer __CDPOUT_goodbye_buffer 2 -s0 -a` takes the contents of `__CDPIN_hello_buffer`, runs the CDP distort program, and puts the resulting audio into `__CDPOUT_goodbye_buffer`.