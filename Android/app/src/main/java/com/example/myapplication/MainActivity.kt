package com.example.myapplication

import android.net.Uri
import android.os.Bundle
import android.os.Environment
import android.util.Log
import android.widget.Toast
import android.widget.VideoView
import androidx.appcompat.app.AppCompatActivity
import com.arthenica.mobileffmpeg.Config
import com.arthenica.mobileffmpeg.FFmpeg
import java.io.File

class MainActivity : AppCompatActivity() {

    private lateinit var videoView: VideoView
    private lateinit var convertedVideoPath: String

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        videoView = findViewById(R.id.videoView)
        val outputDir = getExternalFilesDir(Environment.DIRECTORY_MOVIES)
        convertedVideoPath = "${outputDir?.absolutePath}/output.mp4"

        // Example to start conversion and play
        convertAndPlayVideo()
    }

    private fun convertAndPlayVideo() {
        val inputUri = Uri.parse("android.resource://" + packageName + "/" + R.raw.output)
        val inputPath = getFileFromUri(inputUri)

        if (inputPath != null) {
            Log.d("InputPath", "Input file path: $inputPath")

            val command = arrayOf("-y", "-i", inputPath, "-c:v", "mpeg4", "-c:a", "aac", convertedVideoPath)

            FFmpeg.executeAsync(command) { _, returnCode ->
                if (returnCode == Config.RETURN_CODE_SUCCESS) {
                    playVideo()
                } else {

                    val lastOutput = Config.getLastCommandOutput()
                    Log.d("FFmpegOutput", lastOutput)
                    Toast.makeText(this, "Video conversion failed", Toast.LENGTH_SHORT).show()
                }
            }
        } else {
            Toast.makeText(this, "Video file not found", Toast.LENGTH_SHORT).show()
        }
    }


    private fun playVideo() {
        val videoUri = Uri.parse(convertedVideoPath)
        videoView.setVideoURI(videoUri)
        videoView.start()
    }

    private fun getFileFromUri(uri: Uri): String? {
        val inputStream = contentResolver.openInputStream(uri)
        val file = File.createTempFile("temp_video", ".mpg", cacheDir)
        inputStream?.use { input ->
            file.outputStream().use { output ->
                input.copyTo(output)
            }
        }
        return file.absolutePath
    }
}
