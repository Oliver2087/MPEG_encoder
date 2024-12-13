# MPEG_encoder
The project aims to create an efficient encoder that converts JPEG images into a video file, prioritizing both a higher ratio of images to encoded file size and higher speed for encoding and decoding. The encoder will take multiple JPEGs (up to 100 or more) with the same dimensions and combine them into a single, compact video file while maximizing compression efficiency and processing speed. Playback will be smooth, displaying at least 10 images per second (10 fps). The final synthesized video file adopts the MPEG1 standard.


Other features of this encoder include a command-line interface (CLI) for encoding and viewing images, as well as a graphical user interface (GUI) for selecting and organizing files, applying real-time video effects, and controlling playback, and an Android client which enables an Android device play the MPEG1 video.

# Encoding and Decoding

Encoding and decoding is the main function of this video code. The video codec can transform JPEG images into one video file. During this process, the video codec will perform lossy compression. Ultimately, the codec generates a .mpeg or .mpg file. The codec can also perform decoding, play the video generated by this codec in the rate of 10 images per second.

To implement the encoding process, the codec will load up to 100 JPEG images, encode the JPEG images into a suggested format using lossy compression, and then provide an algorithm to the processed data, by either reducing resolution, cropping the similar concurrent image frames, or controlling the framerate, etc. In this process parameters should be selected to reduce the size of the video format. The processed data is then outputted to the encoder to process into MPEG formatted data.

To implement the decoding process, we use OpenCV packages or libjpeg ot ffmpeg to transfer the processed MPEG video into frames and play these frames so that the video is played.

# MPEG File Format
The video format output by the encoder is MPEG-1. In order to make the output video file conform to the MPEG-1 standard, the file needs to contain the following:

1. Sequence Header: Defines global video properties, including resolution, aspect ratio, frame rate, bitrate, and quantization matrices.
2. GOP Header: Contains information about the Group of Pictures, such as timecodes and whether the group is closed or open.
3. Picture Header: Precedes each frame and provides frame-specific details, such as temporal reference, frame type (I, P, or B), and motion vectors.
4. Slice Data: Encodes the actual image data in macroblocks using motion compensation, DCT coefficients, and run-length encoding.
5. End of Sequence Code: Marks the end of the video stream.
6. Bitstream Encoding: Data is compressed using Huffman coding, zig-zag scan, and variable-length codes.


# Android app
This app is designed to convert MPEG-1 video files to MP4 format and play the converted video.

How to Test with a Different MPEG-1 File?
Replace the existing sample.mpg file in the VideoConverterApp/app/src/main/res/raw folder with your new MPEG-1 video file.
Make sure to name your new file sample.mpg.
Build and run the app.

Target Environment
Device: Pixel 5
API Level: 28
Enjoy using the Video Converter App!
