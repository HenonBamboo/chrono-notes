import QtQuick

Row {
    id: stats

    readonly property ChronoTokens tokens: ChronoTokens {}

    property int totalCount: 0
    property int completedCount: 0
    property color blueColor: tokens.accentBlue
    property color accentColor: tokens.accentYellow

    height: 30
    clip: true
    spacing: 12

    TinyMeta {
        anchors.verticalCenter: parent.verticalCenter
        text: "未完成 " + Math.max(0, stats.totalCount - stats.completedCount)
    }

    TinyMeta {
        anchors.verticalCenter: parent.verticalCenter
        text: "已完成 " + stats.completedCount
    }

    Rectangle {
        width: 138
        height: 7
        radius: 4
        color: tokens.lineSoft
        anchors.verticalCenter: parent.verticalCenter
        clip: true

        Rectangle {
            height: parent.height
            radius: parent.radius
            width: stats.totalCount > 0 ? parent.width * stats.completedCount / stats.totalCount : 0
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: stats.blueColor }
                GradientStop { position: 1.0; color: stats.accentColor }
            }
            Behavior on width { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
        }
    }
}
