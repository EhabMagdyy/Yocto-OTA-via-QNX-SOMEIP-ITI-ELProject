import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    width: 1000
    height: 720
    minimumWidth: 900
    minimumHeight: 600
    title: "OTA Updater"
    color: "#E1E1E1"

    property string selectedImage: ""

    Connections {
        target: otaProcess
        function onLogOutput(text) {
            logArea.text += text
            logArea.cursorPosition = logArea.length
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 15

        // HEADER
        Rectangle {
            Layout.fillWidth: true
            height: 65
            radius: 16
            color: "#FFFFFF"

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                height: 1
                color: "#D6C3DF"
                opacity: 0.35
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 28
                anchors.rightMargin: 28

                Row {
                    spacing: 14
                    Layout.alignment: Qt.AlignVCenter
                    Rectangle {
                        width: 46; height: 46; radius: 14
                        color: "#7D58D9"
                        Text {
                            anchors.centerIn: parent
                            text: "↑"
                            color: "white"
                            font.pixelSize: 24
                            font.bold: true
                        }
                    }
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2
                        Text {
                            text: "OTA Updater"
                            font.pixelSize: 26
                            font.bold: true
                            color: "#1A1A1A"
                        }
                        Text {
                            text: "New Rootfs Deployment Tool"
                            font.pixelSize: 13
                            color: "#7D58D9"
                            font.weight: Font.Medium
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    width: statusCol.implicitWidth + 36
                    height: 42
                    radius: 21
                    color: otaProcess.running ? "#EDB55E" : "#5CB855"
                    opacity: 0.12
                    Layout.alignment: Qt.AlignVCenter

                    Row {
                        id: statusCol
                        anchors.centerIn: parent
                        spacing: 10
                        Rectangle {
                            width: 10; height: 10; radius: 5
                            anchors.verticalCenter: parent.verticalCenter
                            color: otaProcess.running ? "#EDB55E" : "#5CB855"
                            SequentialAnimation on scale {
                                running: otaProcess.running
                                loops: Animation.Infinite
                                NumberAnimation { to: 1.5; duration: 700; easing.type: Easing.InOutQuad }
                                NumberAnimation { to: 1.0; duration: 700; easing.type: Easing.InOutQuad }
                            }
                        }
                    }
                }
            }
        }

        // CARDS ROW
        RowLayout {
            Layout.fillWidth: true
            spacing: 15

            // Target
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 190
                Layout.alignment: Qt.AlignTop
                radius: 16
                color: "#FFFFFF"

                Column {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 15

                    Row {
                        spacing: 10
                        Rectangle { width: 5; height: 20; radius: 3; color: "#7D58D9"; anchors.verticalCenter: parent.verticalCenter }
                        Text { text: "Target Device"; font.pixelSize: 16; font.bold: true; color: "#1A1A1A"; anchors.verticalCenter: parent.verticalCenter }
                    }

                    Column {
                        spacing: 8
                        width: parent.width
                        Text { text: "QNX Device IP Address"; font.pixelSize: 12; color: "#666666"; font.weight: Font.Medium }
                        Rectangle {
                            width: parent.width
                            height: 52
                            radius: 12
                            color: "#F7F7F8"
                            border.width: targetIP.activeFocus ? 2 : 0
                            border.color: "#7D58D9"

                            TextField {
                                id: targetIP
                                anchors.fill: parent
                                anchors.margins: 2
                                text: "192.168.1.15"
                                font.pixelSize: 15
                                color: "#1A1A1A"
                                background: Rectangle { color: "transparent" }
                                leftPadding: 16
                                verticalAlignment: TextInput.AlignVCenter
                                selectByMouse: true
                            }
                        }
                    }
                }
            }

            // Image
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 190
                Layout.alignment: Qt.AlignTop
                radius: 16
                color: "#FFFFFF"

                Column {
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 16

                    Row {
                        spacing: 10
                        Rectangle { width: 5; height: 20; radius: 3; color: "#EDB55E"; anchors.verticalCenter: parent.verticalCenter }
                        Text { text: "Firmware Image"; font.pixelSize: 16; font.bold: true; color: "#1A1A1A"; anchors.verticalCenter: parent.verticalCenter }
                    }

                    Rectangle {
                        width: parent.width
                        height: 56
                        radius: 12
                        color: root.selectedImage !== "" ? "#FFF8E1" : "#F7F7F8"
                        border.width: root.selectedImage !== "" ? 2 : 0
                        border.color: "#EDB55E"

                        Row {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 12

                            Rectangle {
                                width: 32; height: 32; radius: 8
                                color: root.selectedImage !== "" ? "#EDB55E" : "#D6C3DF"
                                anchors.verticalCenter: parent.verticalCenter
                                Text {
                                    anchors.centerIn: parent
                                    text: "💾"
                                    font.pixelSize: 16
                                }
                            }

                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width - 50
                                spacing: 2
                                Text {
                                    text: root.selectedImage !== "" ? "Image Selected" : "No image selected"
                                    font.pixelSize: 13
                                    color: root.selectedImage !== "" ? "#EDB55E" : "#999999"
                                    font.weight: Font.Medium
                                }
                                Text {
                                    text: root.selectedImage !== "" ? root.selectedImage.split('/').pop() : "Browse for .ext4 rootfs"
                                    font.pixelSize: 11
                                    color: root.selectedImage !== "" ? "#1A1A1A" : "#AAAAAA"
                                    elide: Text.ElideMiddle
                                    width: parent.width
                                }
                            }
                        }
                    }

                    Button {
                        width: parent.width
                        height: 44
                        background: Rectangle {
                            radius: 12
                            color: parent.pressed ? "#6A47C0" : "#7D58D9"
                            Behavior on color { ColorAnimation { duration: 120 } }
                        }
                        contentItem: Text {
                            text: "Browse Files"
                            color: "white"
                            font.pixelSize: 14
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: fileDialog.open()
                    }
                }
            }

            // Actions
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 190
                Layout.alignment: Qt.AlignTop
                radius: 16
                color: "#FFFFFF"

                Column {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 15

                    Row {
                        spacing: 10
                        Rectangle { width: 5; height: 20; radius: 3; color: "#5CB855"; anchors.verticalCenter: parent.verticalCenter }
                        Text { text: "Actions"; font.pixelSize: 16; font.bold: true; color: "#1A1A1A"; anchors.verticalCenter: parent.verticalCenter }
                    }

                    Row {
                        width: parent.width
                        spacing: 12

                        Button {
                            width: (parent.width - 12) / 2
                            height: 44
                            enabled: root.selectedImage !== "" && !otaProcess.running
                            background: Rectangle {
                                radius: 12
                                color: !parent.enabled ? "#E8E0F0" : (parent.pressed ? "#4CAF50" : "#5CB855")
                                Behavior on color { ColorAnimation { duration: 120 } }
                            }
                            contentItem: Text {
                                text: "Send OTA"
                                color: !parent.enabled ? "#B0A8C0" : "white"
                                font.pixelSize: 14
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: {
                                logArea.text = ""
                                otaProcess.start("bash", [
                                    "/home/ehab/Documents/ITI_9Months/Courses-Project/Embedded-Linux/Final/send-ota.sh",
                                    targetIP.text,
                                    root.selectedImage
                                ])
                            }
                        }

                        Button {
                            width: (parent.width - 12) / 2
                            height: 44
                            enabled: otaProcess.running
                            background: Rectangle {
                                radius: 12
                                color: !parent.enabled ? "#F5F5F5" : (parent.pressed ? "#C01010" : "#E01305")
                                Behavior on color { ColorAnimation { duration: 120 } }
                            }
                            contentItem: Text {
                                text: "Cancel"
                                color: !parent.enabled ? "#CCCCCC" : "white"
                                font.pixelSize: 14
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: otaProcess.kill()
                        }
                    }

                    Text {
                        text: otaProcess.running ? "⚠ Do not close while deploying" : "Select an image and target to begin"
                        font.pixelSize: 11
                        color: otaProcess.running ? "#EDB55E" : "#999999"
                        font.weight: Font.Medium
                        width: parent.width
                        wrapMode: Text.Wrap
                    }
                }
            }
        }

        // TERMINAL
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 220
            radius: 16
            color: "#111111"
            clip: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                /* Title bar */
                Rectangle {
                    Layout.fillWidth: true
                    height: 44
                    color: "#1E1E1E"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 20
                        spacing: 12

                        Row {
                            spacing: 8
                            Layout.alignment: Qt.AlignVCenter
                            Rectangle { width: 12; height: 12; radius: 6; color: "#E01305" }
                            Rectangle { width: 12; height: 12; radius: 6; color: "#EDB55E" }
                            Rectangle { width: 12; height: 12; radius: 6; color: "#5CB855" }
                        }

                        Text {
                            text: "Deployment Console"
                            color: "#888888"
                            font.pixelSize: 13
                            font.bold: true
                            font.family: "Monospace"
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Item { Layout.fillWidth: true }

                        Rectangle {
                            width: clearLbl.implicitWidth + 24
                            height: 28
                            radius: 6
                            color: clearMouse.containsMouse ? "#333333" : "transparent"
                            Layout.alignment: Qt.AlignVCenter

                            Text {
                                id: clearLbl
                                anchors.centerIn: parent
                                text: "Clear"
                                color: "#AAAAAA"
                                font.pixelSize: 12
                                font.family: "Monospace"
                            }
                            MouseArea {
                                id: clearMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: logArea.text = ""
                            }
                        }

                        Rectangle {
                            width: badge.implicitWidth + 20
                            height: 24
                            radius: 12
                            color: otaProcess.running ? "#EDB55E" : "#5CB855"
                            opacity: 0.15
                            Layout.alignment: Qt.AlignVCenter

                            Text {
                                id: badge
                                anchors.centerIn: parent
                                text: otaProcess.running ? "● LIVE" : "● IDLE"
                                color: otaProcess.running ? "#EDB55E" : "#5CB855"
                                font.pixelSize: 11
                                font.bold: true
                                font.family: "Monospace"
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "#333333"
                }

                // Terminal body
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#111111"

                    ScrollView {
                        id: terminalScroll
                        anchors.fill: parent
                        anchors.margins: 16
                        clip: true

                        TextArea {
                            id: logArea
                            width: terminalScroll.availableWidth
                            height: contentHeight
                            readOnly: true
                            font.family: "Monospace"
                            font.pixelSize: 13
                            wrapMode: TextArea.Wrap
                            background: Rectangle { color: "transparent" }
                            color: "#D4D4D4"
                            selectByMouse: true
                            padding: 0
                        }

                        ScrollBar.vertical: ScrollBar {
                            width: 8
                            contentItem: Rectangle {
                                implicitWidth: 8
                                radius: 4
                                color: "#444444"
                            }
                        }
                        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOff }
                    }

                    Text {
                        visible: logArea.text === ""
                        anchors.centerIn: parent
                        text: "Logs will appear here…"
                        color: "#555555"
                        font.family: "Monospace"
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        lineHeight: 1.4
                    }
                }
            }
        }

        // PROGRESS BAR
        Rectangle {
            Layout.fillWidth: true
            height: 70
            radius: 16
            color: "#FFFFFF"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 15
                anchors.rightMargin: 30
                spacing: 15

                Column {
                    Layout.fillWidth: true
                    spacing: 10

                    Row {
                        spacing: 10
                        Rectangle { width: 5; height: 18; radius: 3; color: "#7D58D9"; anchors.verticalCenter: parent.verticalCenter }
                        Text {
                            text: "Transfer Progress"
                            font.pixelSize: 15
                            font.bold: true
                            color: "#1A1A1A"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    // Progress track
                    Rectangle {
                        width: parent.width - 10
                        height: 10
                        radius: 7
                        color: "#F0F0F0"
                        clip: true

                        // Determinate fill
                        Rectangle {
                            width: parent.width * (otaProcess.progress / 100)
                            height: parent.height
                            radius: 7
                            color: "#7D58D9"
                            visible: !indeterminateAnim.running
                            Behavior on width { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
                        }

                        // Indeterminate sweep when running but 0%
                        Rectangle {
                            id: indeterminateBar
                            width: parent.width * 0.35
                            height: parent.height
                            radius: 7
                            color: "#7D58D9"
                            visible: indeterminateAnim.running
                            x: -width

                            SequentialAnimation on x {
                                id: indeterminateAnim
                                running: otaProcess.running && otaProcess.progress === 0
                                loops: Animation.Infinite
                                NumberAnimation { to: indeterminateBar.width; duration: 1200; easing.type: Easing.InOutQuad }
                                NumberAnimation { from: -indeterminateBar.width; to: -indeterminateBar.width; duration: 0 }
                            }
                        }
                    }
                }

                // Percentage pill
                Rectangle {
                    Layout.alignment: Qt.AlignVCenter
                    width: 60
                    height: 40
                    radius: 10
                    color: otaProcess.running ? "#F5F0FF" : "#F7F7F8"

                    Text {
                        id: percentTxt
                        anchors.centerIn: parent
                        text: otaProcess.running ? otaProcess.progress + "%" : "—"
                        font.pixelSize: 16
                        font.bold: true
                        color: otaProcess.running ? "#7D58D9" : "#AAAAAA"
                        font.family: "Monospace"
                    }
                }
            }
        }
    }

    // FILE DIALOG
    FileDialog {
        id: fileDialog
        title: "Select rootfs image"
        currentFolder: "file:///home/ehab/Documents/ITI_9Months/Yocto/shared-build/tmp-glibc/deploy/images/raspberrypi3-64"
        nameFilters: ["ext4 images (*.ext4)", "All files (*)"]
        onAccepted: {
            var url = fileDialog.selectedFile.toString()
            root.selectedImage = url.replace(/^file:\/{2,3}/, "")
        }
    }
}