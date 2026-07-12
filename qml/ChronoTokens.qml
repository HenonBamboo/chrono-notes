import QtQuick

QtObject {
    readonly property color paper: "#fff5af"
    readonly property color paperSoft: "#fff8c8"
    readonly property color paperEdge: "#f4df73"
    readonly property color paperProject: "#e9f7d7"
    readonly property color mintSoft: "#f2fbeb"
    readonly property color card: "#fffef7"
    readonly property color cardQuiet: "#fffbea"
    readonly property color ink: "#071426"
    readonly property color muted: "#64748b"
    readonly property color mutedSoft: "#8a97a8"
    readonly property color line: "#1f071426"
    readonly property color lineSoft: "#13071426"
    readonly property color accentYellow: "#f4bf30"
    readonly property color accentYellowSoft: "#ffe58d"
    readonly property color accentBlue: "#2d68c7"
    readonly property color accentBlueSoft: "#e8f0ff"
    readonly property color accentMint: "#88c57f"
    readonly property color lightGlow: "#66ffffff"
    readonly property color lightRing: "#44d7ead0"
    readonly property color activeGlow: "#55d7ead0"
    readonly property color danger: "#b4534a"
    readonly property color dangerSoft: "#ffe7df"
    readonly property color drawerPaper: "#f6f1dc"
    readonly property color drawerCard: "#fffdf3"

    readonly property int space1: 4
    readonly property int space2: 8
    readonly property int space3: 12
    readonly property int space4: 16
    readonly property int space5: 20
    readonly property int space6: 24

    readonly property int radiusXs: 6
    readonly property int radiusSm: 9
    readonly property int radiusMd: 13
    readonly property int radiusLg: 18
    readonly property int radiusPill: 999

    property string fontUi: "Microsoft YaHei UI"
    readonly property string fontMono: "Cascadia Mono"
    property int baseFontSize: 12
    readonly property int sizeMeta: Math.max(10, baseFontSize - 1)
    readonly property int sizeBody: baseFontSize
    readonly property int sizeTitle: baseFontSize + 2
    readonly property int motionFast: 120
    readonly property int motionMedium: 180
    readonly property int motionSlow: 220
    readonly property int drawerDuration: motionMedium
}
