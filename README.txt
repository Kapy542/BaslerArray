No x86 configuration

Ubuntu:
In /BaslerArray/SynchronizedSnapshots run:
sudo mkdir build
cd build
cmake ..
make

1. Run SynchronizedSnapshots as:
	cout << "Press w to write image" << endl 
             << "Press r to START recording" << endl
             << "Press t to STOP recording" << endl
             << "Press ESC or q to exit..." << endl;

2. After exiting, the images are written in "./recording/take_name/cam_idx/"
3. In /BaslerArray Run: ' python ./raw2png.py "path_to_build/recording/take_name" '
	to convert binary image files into pngs