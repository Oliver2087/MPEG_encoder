package com.example.converterapp

import android.net.Uri
import android.os.Bundle
import android.os.Environment
import android.util.Log
import android.widget.Button
import android.widget.Toast
import android.widget.VideoView
import androidx.appcompat.app.AppCompatActivity
import com.arthenica.mobileffmpeg.FFmpeg
import java.io.File

class MainActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        val playButton = findViewById<Button>(R.id.playButton)
        val videoView = findViewById<VideoView>(R.id.videoView)
        val outputPath = File(getExternalFilesDir(Environment.DIRECTORY_MOVIES), "sample.mp4").absolutePath

        playButton.setOnClickListener {
            val localInputPath = copyRawResourceToLocalPath(
                R.raw.sample,
                File(getExternalFilesDir(Environment.DIRECTORY_MOVIES), "sample.mpg").absolutePath
            )

            val outputFile = File(outputPath)
            if (outputFile.exists()) {
                outputFile.delete() // 删除已存在的目标文件
            }

            val ffmpegCommand = arrayOf(
                "-i", localInputPath,
                "-c:v", "mpeg4",

                "-c:a", "aac",
                "-strict", "experimental",
                outputPath
            )

            Toast.makeText(this, "Converting video, please wait...", Toast.LENGTH_SHORT).show()

            FFmpeg.executeAsync(ffmpegCommand) { executionId, returnCode ->
                if (returnCode == 0) {
                    Log.d("FFmpeg", "Video conversion succeeded")
                    runOnUiThread {
                        Toast.makeText(this, "Conversion succeeded! Playing video...", Toast.LENGTH_SHORT).show()
                        videoView.setVideoURI(Uri.parse(outputPath))
                        videoView.start()
                    }
                } else {
                    Log.e("FFmpeg", "Video conversion failed with returnCode: $returnCode")
                    runOnUiThread {
                        Toast.makeText(this, "Video conversion failed! Please try again.", Toast.LENGTH_SHORT).show()
                    }
                }
            }
        }
    }

    private fun copyRawResourceToLocalPath(resourceId: Int, outputFilePath: String): String {
        val inputStream = resources.openRawResource(resourceId)
        val outputFile = File(outputFilePath)
        val outputStream = outputFile.outputStream()
        inputStream.use { input ->
            outputStream.use { output ->
                input.copyTo(output)
            }
        }
        return outputFile.absolutePath
    }
}
