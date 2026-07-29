import QtQuick

QtObject {
    // Core light theme: warm paper surfaces, restrained amber brand color,
    // sage project semantics, and a dedicated blue focus indicator.
    readonly property color canvas: "#F7F2E8"
    readonly property color surface: "#FFFDF8"
    readonly property color surfaceMuted: "#F2EBDD"
    readonly property color ink: "#202723"
    readonly property color textSecondary: "#5E6862"
    readonly property color border: "#D8D0C3"
    readonly property color accent: "#82551F"
    readonly property color accentSoft: "#F1DFC0"
    readonly property color projectAccent: "#58705A"
    readonly property color focus: "#245EA8"
    readonly property color danger: "#9B3730"
    readonly property color dangerSoft: "#F8E2DF"

    // Semantic state colors. Components should consume these instead of
    // inventing local hex values.
    readonly property color transparent: "#00000000"
    readonly property color textDisabled: "#8A928D"
    readonly property color textOnAccent: "#FFFDF8"
    readonly property color surfaceHover: "#FBF6EC"
    readonly property color surfacePressed: "#EEE5D5"
    readonly property color surfaceDisabled: "#F3EFE7"
    readonly property color borderSubtle: "#80D8D0C3"
    readonly property color borderStrong: "#B9AE9D"
    readonly property color accentHover: "#704719"
    readonly property color accentPressed: "#5C3A16"
    readonly property color projectAccentSoft: "#E4EBE1"
    readonly property color focusSoft: "#E5EDF8"
    readonly property color focusRing: "#245EA8"
    readonly property color completedSurface: "#EEF2ED"
    readonly property color archiveSurface: "#F4EFE5"
    readonly property color projectCanvas: "#F1F4ED"
    readonly property color scrim: "#2E202723"
    readonly property color toastSurface: "#202723"
    readonly property color toastText: "#FFFDF8"
    readonly property color selectionSurface: "#D9E4EBE1"
    readonly property color insetSurface: "#B8FFFDF8"
    readonly property color progressTrack: "#D8E4EBE1"
    readonly property color projectLine: "#7058705A"

    // Compatibility aliases for the existing QML surface. Keep these until
    // every consumer has migrated to semantic names.
    readonly property color paper: canvas
    readonly property color paperSoft: surfaceMuted
    readonly property color paperEdge: border
    readonly property color paperProject: projectCanvas
    readonly property color mintSoft: projectAccentSoft
    readonly property color card: surface
    readonly property color cardQuiet: surfaceMuted
    readonly property color muted: textSecondary
    readonly property color mutedSoft: textDisabled
    readonly property color line: border
    readonly property color lineSoft: borderSubtle
    readonly property color accentYellow: accent
    readonly property color accentYellowSoft: accentSoft
    readonly property color accentBlue: focus
    readonly property color accentBlueSoft: focusSoft
    readonly property color accentMint: projectAccent
    readonly property color lightGlow: insetSurface
    readonly property color lightRing: focusSoft
    readonly property color activeGlow: accentSoft
    readonly property color drawerPaper: surfaceMuted
    readonly property color drawerCard: surface

    readonly property int space1: 4
    readonly property int space2: 8
    readonly property int space3: 12
    readonly property int space4: 16
    readonly property int space5: 24
    readonly property int space6: 32

    readonly property int radiusXs: 8
    readonly property int radiusSm: 8
    readonly property int radiusMd: 12
    readonly property int radiusLg: 16
    readonly property int radiusPill: 999

    property string fontUi: "Microsoft YaHei UI"
    property bool reduceMotion: false
    readonly property string fontMono: "Cascadia Mono"
    property int baseFontSize: 14
    readonly property int sizeMeta: Math.max(12, baseFontSize - 2)
    readonly property int sizeBody: baseFontSize
    readonly property int sizeLabel: Math.max(13, baseFontSize - 1)
    readonly property int sizeHeading: baseFontSize + 2
    readonly property int sizeTitle: sizeHeading
    readonly property int sizeDisplay: baseFontSize + 6

    readonly property int controlHeight: 40
    readonly property int primaryControlHeight: 44
    readonly property int motionFast: reduceMotion ? 0 : 120
    readonly property int motionMedium: reduceMotion ? 0 : 140
    readonly property int motionSlow: reduceMotion ? 0 : 200
    readonly property int drawerDuration: motionSlow
}
