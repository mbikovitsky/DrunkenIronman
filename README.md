# DrunkenIronman
Taking the Death out of the Blue Screen.


## Features
- Taking screenshots of BSoDs in the wild. ([Documentation](Docs/VgaDump.md)).
- Customizing the message displayed on the Blue Screen.
  ([Documentation](Docs/Carpenter.md)).


## Usage
Make sure memory dumps are enabled if you intend
to capture screenshots.

The software has been tested only on Windows XP SP3. YMMV.

```
DrunkenIronman.exe <subfunction> <subfunction args>

  convert [<input>] <output>
    Extracts a screenshot from a memory dump.

  load
    Loads the driver.

  unload
    Unloads the driver.

  bugshot
    Instructs the driver to capture a screenshot
    of the next BSoD.

  vanity <string>
    Crashes the system and displays the specified string
    on the BSoD.

  qr
    Displays the dimensions of the current QR image.

  qr <image>
    Sets an image to be used instead of the default QR code.
    The image must be a non-compressed BMP
    with either 32 or 24 BPP, and with the same dimensions
    as the default QR image.
```

### Examples

#### Conversion
```
DrunkenIronman.exe convert out.bmp
DrunkenIronman.exe convert C:\Some\Path\MEMORY.DMP out2.bmp
```

#### Custom Bugcheck Message
```
DrunkenIronman.exe vanity IRQL_NOT_LESS_OR_AWESOME
```

#### Custom QR image
```
DrunkenIronman.exe qr C:\Some\Path\image.bmp
```


## Screenshots
![IRQL_NOT_LESS_OR_AWESOME](Screenshot.bmp "IRQL_NOT_LESS_OR_AWESOME")
