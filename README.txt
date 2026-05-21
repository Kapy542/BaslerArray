No x86 configuration

1. Run SynchronizedSnapshots as:
	cout << "Press w to write image" << endl 
             << "Press r to START recording" << endl
             << "Press t to STOP recording" << endl
             << "Press ESC or q to exit..." << endl;

2. After exiting, the images are written in "./recording/take_name/cam_idx/"
3. Run: python raw2png.py "./recording/take_name" 
	to convert binary image files into pngs