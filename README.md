A linux tool to recover files from NTFS partition.\
Opens device file, partition file or just a file like image in read only mode.

To build run `git clone git@github.com:Kontra2B/NTFS-recovery.git`\
Enter checked folder with `cd NTFS-recovery` and run `make`.

For help run: `./ntfs.recover -h`\
To recover files from /dev/sdx (no partition required) to current folder run: `./ntfs.recover /dev/sdx -R`\
Example of 10 pictures recovery and console output:

`> ./ntfs.recover -R -i image -t recovered -s10 -l0x601234 /dev/sdb1`
```
Parsed arguments:
Device:/dev/sdb1, LBA:6296116x601234, process:10, include[image] pid:117565
RECOVER to target dir: recovered

	Press enter to confirm...

Searching for MFT entries...

New context LBA bias based on last $MFT record: 0x0@6291456x600000
x601234 file/2330       /Nasze zdjęcia/2003/'03_09_07/DSC02027.JPG      2003.09.06 18:24:08     size:1328k      333:x6DB470/x6DBED8
x601236 file/2331       /Nasze zdjęcia/2003/'03_09_07/DSC02028.JPG      2003.09.06 18:24:28     size:1365k      342:x6DBED8/x6DC988
x601238 file/2332       /Nasze zdjęcia/2003/'03_09_07/DSC02029.JPG      2003.09.06 18:24:50     size:1381k      346:x6DC988/x6DD458
x60123A file/2333       /Nasze zdjęcia/2003/'03_09_07/DSC02030.JPG      2003.09.06 18:27:38     size:1342k      336:x6DD458/x6DDED8
x60123C file/2334       /Nasze zdjęcia/2003/'03_09_07/DSC02031.JPG      2003.09.06 20:19:22     size:1246k      312:x6DDED8/x6DE898
x60123E file/2335       /Nasze zdjęcia/2003/'03_09_07/DSC02032.JPG      2003.09.06 20:19:44     size:1181k      296:x6DE898/x6DF1D8
x601240 file/2336       /Nasze zdjęcia/2003/'03_09_07/DSC02033.JPG      2008.11.16 23:43:39     size:1328k      333:x6DF1D8/x6DFC40
x601242 file/2337       /Nasze zdjęcia/2003/'03_09_07/DSC02034.JPG      2003.09.06 20:24:58     size:1436k      360:x6DFC40/x6E0780
x601244 file/2338       /Nasze zdjęcia/2003/'03_09_07/DSC02036.JPG      2003.09.06 21:06:52     size:1238k      310:x6E0780/x6E1130
x601246 file/2339       /Nasze zdjęcia/2003/'03_09_07/DSC02037.JPG      2003.09.06 22:10:22     size:1373k      344:x6E1130/x6E1BF0

Wait for child processes... 
```

Have you recovered files from a corrupted partition?\
Or have you recovered files from a reformatted partition?\
Or recovered deleted files successfully?\
Or just enjoyed inspecting data with debugging options -vv?

[![Buy Me A Coffee](https://img.shields.io/badge/Buy%20Me%20A%20Coffee-donate-yellow?style=flat-square&logo=buy-me-a-coffee)](https://www.buymeacoffee.com/kontra)
