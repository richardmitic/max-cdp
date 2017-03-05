# MaxCDP
A MaxMSP frontend for the [Composers Desktop Project](http://www.unstablesound.net/cdp.html)

## Info
Based on the the "threadpool" example object in the Max SDK.

### Building 
Clone the [Max SDK](https://github.com/Cycling74/max-sdk), then clone this repo in `source/advanced`. Build with Xcode. Make sure the `externals` directory is added to the Max search path.

### Using
CDP must be installed somewhere on your system. If it's not in the Max search path you can specify the root directory with the `@root` attribute.

Send messages to the object that look exactly like a CDP shell command, e.g. `distort fractal /path/to/sound.wav /path/to.sound.out.wav 3 0.5 -p0.5`