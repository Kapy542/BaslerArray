No x86 configuration

Ubuntu:
In /BaslerArray/SynchronizedSnapshots run:
mkdir build
cd build
cmake ..
make
cp -r ../configs ./configs

1. In builöd folder, Run SynchronizedSnapshots as: ./SynchronizedSnapshots
	cout << "Press w to write image" << endl 
             << "Press r to START recording" << endl
             << "Press t to STOP recording" << endl
             << "Press ESC or q to exit..." << endl;

2. After exiting, the images are written in "./recordings/take_name/cam_idx/"
3. In /BaslerArray Run: ' python ./raw2png.py "path_to_build/recordings/take_name" '
	to convert binary image files into pngs
