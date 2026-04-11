#Requires AutoHotkey v2.0
#SingleInstance Force

recentEvents := []

showEvent(text) {
    global recentEvents

    recentEvents.Push(text)
    while (recentEvents.Length > 4) {
        recentEvents.RemoveAt(1)
    }

    displayText := ""
    for index, eventText in recentEvents {
        if (index > 1) {
            displayText .= "`n"
        }
        displayText .= eventText
    }

    ToolTip displayText
    SetTimer () => ToolTip(), -1500
}

F13:: {
    showEvent("F13 down")
}

F13 Up:: {
    showEvent("F13 up")
}

F14:: {
    showEvent("F14 down")
}

F14 Up:: {
    showEvent("F14 up")
}
