import QtQuick 2.12
import QtQuick.Controls 2.12
import QtGraphicalEffects 1.12

Item {
    anchors.fill: parent

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#6dd5fa" }
            GradientStop { position: 1.0; color: "#2980b9" }
        }

        Rectangle {
            id: btn
            width: 180
            height: 60
            radius: 12
            anchors.centerIn: parent
            color: "#ffffff"

            border.color: "#dddddd"

            Text {
                anchors.centerIn: parent
                text: "待开发"
                font.pixelSize: 20
                color: "#333"
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true

                onClicked: {
                    factory_analyzer.qmlstartTest()
                }

                onPressed: {
                    btn.scale = 0.95
                }

                onReleased: {
                    btn.scale = 1.0
                }

                onEntered: {
                    btn.color = "#f2f2f2"
                }

                onExited: {
                    btn.color = "#ffffff"
                }
            }

            Behavior on scale {
                NumberAnimation {
                    duration: 100
                    easing.type: Easing.OutQuad
                }
            }
        }

        DropShadow {
            anchors.fill: btn
            source: btn
            radius: 12
            samples: 16
            color: "#80000000"
        }
    }
}
