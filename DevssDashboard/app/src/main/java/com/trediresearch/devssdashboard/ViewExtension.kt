package com.trediresearch.devssdashboard

import android.os.SystemClock
import android.view.View

fun View.setOnDoubleClickListener(doubleClickInterval: Long = 300L, onDoubleClick: (View) -> Unit) {
    var lastClickTime: Long = 0

    this.setOnClickListener { view ->
        val currentTime = SystemClock.elapsedRealtime()
        if (currentTime - lastClickTime < doubleClickInterval) {
            onDoubleClick(view)
            lastClickTime = 0 // Reset per evitare che un terzo tap riattivi il doppio click
        } else {
            lastClickTime = currentTime
        }
    }
}