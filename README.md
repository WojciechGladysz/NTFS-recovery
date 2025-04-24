A linux tool to recover files from NTFS file system.\
Opens device file, partition file or just a file like image in read only mode.\
You need to be a member of disk group to access disk or partition device file.

To build run `git clone git@github.com:Kontra2B/NTFS-recovery.git`\
Enter checked folder with `cd NTFS-recovery` and run `make`.

For help run: `./ntfs.recover -h`\
To recover files from /dev/sdx (no partition required) to current folder run: `./ntfs.recover /dev/sdx -R`\
Add -u option to handle deleted files.\
Example of recovery of 4 pictures with console output:

`> ./ntfs.recover /dev/sda2 -s4 -iimage -Rt recover`
```
Parsed arguments:
Device:/dev/sda2, LBA:0x0, process:4, only[image] pid:2182708
RECOVER to target dir: recover

	Press enter to confirm...

Searching for MFT entries...

New context LBA bias based on last $MFT record: 0x0@6291456x600000
x600088 file/68 /zdjęcia/Redmi 9T/IMG_20230402_161627.jpg       2023.11.24 22:16:02     size:2747k      687:x426F4C0/x4270A38
x60008E file/71 /zdjęcia/Redmi 9T/IMG_20220707_122243.jpg       2023.11.24 21:54:33     size:3285k      822:x42774C0/x4278E70
x600094 file/74 /zdjęcia/Redmi 9T/IMG_20221207_154630.jpg       2023.11.24 22:13:41     size:3324k      832:x4297EF0/x42998F0
x600096 file/75 /zdjęcia/FujiFilm X-A20/2020/Oporto/DSCF6734.JPG    2020.10.08 16:09:06     size:2439k      610:x4297BC0/x4297DD0   x42B2A38/x42B3B38

Wait for child processes... 
```

Have you recovered files from a corrupted partition?\
Or have you recovered files from a re-formatted partition?\
Or recovered deleted files from a good partition successfully?\
Or just enjoyed inspecting data with debugging options -vv?

[![Buy Me A Coffee](https://img.shields.io/badge/Buy%20Me%20A%20Coffee-donate-yellow?style=flat-square&logo=buy-me-a-coffee)](https://www.buymeacoffee.com/kontra)
