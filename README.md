A linux tool to recover files from NTFS partition.

Opens device file, partition file or just a file like image in read only mode.

Run git clone git@github.com:Kontra2B/NTFS-recovery.git
cd to NTFS-recovery and run make.

For help run: ./ntfs.recover -h

To recover files from /dev/sdx (no partition required) to current folder run:

./ntfs.recover /dev/sdx -R

Example of 10 pictures recovery and console output:

> ./ntfs.recover -R -iimage -t recovered -s10 -l0x612345 /dev/sdb1
Parsed arguments:
Device:/dev/sdb1, LBA:6366021x612345, process:10, include[image] pid:113919
RECOVER to target dir: recovered

	Press enter to confirm...

Searching for MFT entries...

New context LBA bias based on last $MFT record: 0x0@6291456x600000
x612904 file/38018      /Moje zdjęcia/2021/Lagos/październik/20/DSCF9810.JPG    2021.10.16 11:01:50     size:5963k      1491:x36557280/x365585C0        x365C9580/x365CAC10     x36643750/x36643C18	
x612906 file/38019      /Moje zdjęcia/2021/Lagos/październik/20/DSCF9811.JPG    2021.10.16 11:04:56     size:5087k      1272:x36653CF0/x366564B0
x612908 file/38020      /Moje zdjęcia/2021/Lagos/październik/20/DSCF9812.JPG    2021.10.16 11:05:20     size:4725k      1182:x3668CE50/x3668EA00        x366B47C0/x366B5100
x61290A file/38021      /Moje zdjęcia/2021/Lagos/październik/20/DSCF9813.JPG    2021.10.16 11:05:56     size:6124k      1532:x366EC8E0/x366EEA00        x36700868/x36701728
x61290C file/38022      /Moje zdjęcia/2021/Lagos/październik/20/DSCF9814.JPG    2021.10.16 11:06:02     size:6485k      1622:x3670C328/x3670EA00        x36745F70/x36746A00     x3678A210/x3678A358	
x61290E file/38023      /Moje zdjęcia/2021/Lagos/październik/20/DSCF9815.JPG    2021.10.16 11:07:04     size:6298k      1575:x367C5198/x367C6A00        x367DC690/x367DDF60
x612910 file/38024      /Moje zdjęcia/2021/Lagos/październik/20/DSCF9816.JPG    2021.10.16 11:07:22     size:6012k      1504:x367E4740/x367E6A00        x368002B0/x36800700     x36840AB8/x368412A8	
x612912 file/38025      /Moje zdjęcia/2021/Lagos/październik/20/DSCF9817.JPG    2021.10.16 11:10:02     size:5225k      1307:x3684C880/x3684EA00        x36870958/x368710B0
x612914 file/38026      /Moje zdjęcia/2021/Lagos/październik/29/DSCF0116.JPG    2021.10.20 11:56:16     size:5967k      1492:x26F1E278/x26F1F350        x26F267B8/x26F276C0     x26F468E8/x26F476C0	x26F4F110/x26F4F1F8	
x612916 file/38027      /Moje zdjęcia/2021/Lagos/październik/20/DSCF9819.JPG    2021.10.16 11:10:32     size:4154k      1039:x36905690/x36906A00        x36940770/x36941478

Wait for child processes...
