import QtQuick

Row {
    id: stats

    readonly property ChronoTokens fallbackTokens: ChronoTokens {
        fontUi: stats.uiFontFamily
        baseFontSize: stats.uiFontSize
    }
    readonly property var tokens: stats.theme ? stats.theme : stats.fallbackTokens

    property int totalCount: 0
    property int completedCount: 0
    property color blueColor: stats.tokens.accentBlue
    property color accentColor: stats.tokens.accentYellow
    property var theme: null
    property string uiFontFamily: theme ? theme.fontUi : "Microsoft YaHei UI"
    property int uiFontSize: theme ? theme.baseFontSize : 12

    height: 36
    clip: true
    spacing: 12

    TinyMeta {
        anchors.verticalCenter: parent.verticalCenter
        text: "未完成 " + Math.max(0, stats.totalCount - stats.completedCount)
        uiFontFamily: stats.tokens.fontUi
        uiFontSize: stats.tokens.sizeMeta
        theme: stats.tokens
        textColor: stats.tokens.textSecondary
        font.family: stats.tokens.fontUi
        font.pixelSize: stats.tokens.sizeMeta
    }

    TinyMeta {
        anchors.verticalCenter: parent.verticalCenter
        text: "已完成 " + stats.completedCount
        uiFontFamily: stats.tokens.fontUi
        uiFontSize: stats.tokens.sizeMeta
        theme: stats.tokens
        textColor: stats.tokens.textSecondary
        font.family: stats.tokens.fontUi
        font.pixelSize: stats.tokens.sizeMeta
    }

    Rectangle {
        width: 138
        height: 7
        radius: 4
        color: stats.tokens.lineSoft
        anchors.verticalCenter: parent.verticalCenter
        clip: true

        Accessible.role: Accessible.ProgressBar
        Accessible.name: stats.totalCount > 0
                         ? "便签完成进度 " + Math.round(stats.completedCount * 100 / stats.totalCount) + "%"
                         : "便签完成进度 0%"
        Accessible.description: "已完成 " + stats.completedCount + " 条，共 " + stats.totalCount + " 条"

        Rectangle {
            height: parent.height
            radius: parent.radius
            width: stats.totalCount > 0 ? parent.width * stats.completedCount / stats.totalCount : 0
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: stats.blueColor }
                GradientStop { position: 1.0; color: stats.accentColor }
            }
            Behavior on width { NumberAnimation { duration: stats.tokens.motionSlow; easing.type: Easing.OutCubic } }
        }
    }
}
