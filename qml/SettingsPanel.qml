pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: settingsPanel
    objectName: "settingsPanelSurface"

    readonly property ChronoTokens tokens: ChronoTokens {
        fontUi: settingsPanel.uiFontFamily
        baseFontSize: settingsPanel.uiFontSize
    }

    property alias apiUrl: apiUrlField.text
    property alias apiKey: apiKeyField.text
    property alias modelName: modelField.text
    property string uiFontFamily: "Microsoft YaHei UI"
    property int uiFontSize: 12
    property var uiFontFamilies: ["Microsoft YaHei UI"]
    property bool clearAllArmed: false
    property bool apiExpanded: true
    property bool appearanceExpanded: true
    property bool dataExpanded: true
    property bool inputActiveFocus: apiUrlField.activeFocus || apiKeyField.activeFocus || modelField.activeFocus ||
                                    fontFamilyCombo.activeFocus || fontSizeBox.activeFocus
    property color surfaceColor: tokens.drawerPaper
    readonly property var fontPickerModel: uiFontFamilies && uiFontFamilies.length > 0
                                           ? uiFontFamilies
                                           : [uiFontFamily.length > 0 ? uiFontFamily : "Microsoft YaHei UI"]

    signal saveRequested(string url, string key, string model, string fontFamily, int fontSize)
    signal clearCompletedRequested()
    signal clearCompletedAllRequested()
    signal clearCurrentRequested()
    signal clearAllRequested()
    signal exportJsonRequested()
    signal importJsonRequested()
    signal exportMarkdownRequested()

    color: surfaceColor
    radius: tokens.radiusLg
    border.color: Qt.rgba(106 / 255, 138 / 255, 91 / 255, 0.18)
    border.width: 1

    function fontIndex(family) {
        for (let index = 0; index < fontPickerModel.length; ++index) {
            if (String(fontPickerModel[index]).toLowerCase() === family.toLowerCase())
                return index
        }
        return -1
    }

    function syncFontControls() {
        const target = uiFontFamily.length > 0 ? uiFontFamily : "Microsoft YaHei UI"
        const index = fontIndex(target)
        fontFamilyCombo.currentIndex = index >= 0 ? index : 0
        if (fontSizeBox.value !== uiFontSize)
            fontSizeBox.value = uiFontSize
    }

    function releaseInputFocus() {
        apiUrlField.focus = false
        apiKeyField.focus = false
        modelField.focus = false
        fontFamilyCombo.focus = false
        fontSizeBox.focus = false
    }

    function resetTextViews() {
        releaseInputFocus()
        apiUrlField.cursorPosition = 0
        apiKeyField.cursorPosition = 0
        modelField.cursorPosition = 0
    }

    function apiUrlCursorPosition() {
        return apiUrlField.cursorPosition
    }

    function apiSectionExpanded() {
        return apiExpanded
    }

    function appearanceSectionExpanded() {
        return appearanceExpanded
    }

    function dataSectionExpanded() {
        return dataExpanded
    }

    function toggleApiSection() {
        apiExpanded = !apiExpanded
    }

    function toggleAppearanceSection() {
        appearanceExpanded = !appearanceExpanded
    }

    function toggleDataSection() {
        dataExpanded = !dataExpanded
    }

    onUiFontFamilyChanged: Qt.callLater(syncFontControls)
    onUiFontSizeChanged: Qt.callLater(syncFontControls)
    onUiFontFamiliesChanged: Qt.callLater(syncFontControls)
    Component.onCompleted: syncFontControls()

    component SettingsField: TextField {
        id: field
        selectByMouse: true
        font.pixelSize: settingsPanel.tokens.sizeBody + 1
        font.family: settingsPanel.tokens.fontUi
        color: settingsPanel.tokens.ink
        placeholderTextColor: settingsPanel.tokens.mutedSoft
        selectionColor: settingsPanel.tokens.accentYellowSoft
        selectedTextColor: settingsPanel.tokens.ink
        background: Rectangle {
            radius: settingsPanel.tokens.radiusSm
            color: field.activeFocus ? "#f8fff7" : "#ccfffef7"
            border.width: 1
            border.color: field.activeFocus ? settingsPanel.tokens.accentMint : settingsPanel.tokens.lineSoft
            Behavior on color { ColorAnimation { duration: settingsPanel.tokens.motionFast; easing.type: Easing.OutCubic } }
            Behavior on border.color { ColorAnimation { duration: settingsPanel.tokens.motionFast; easing.type: Easing.OutCubic } }
        }
    }

    component SettingsCard: Rectangle {
        property alias content: slot.data

        Layout.fillWidth: true
        color: settingsPanel.tokens.drawerCard
        radius: settingsPanel.tokens.radiusMd
        border.color: "#2288c57f"
        implicitHeight: slot.implicitHeight + 28

        ColumnLayout {
            id: slot
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10
        }
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 20
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: settingsPanel.width - 40
            spacing: 14

            Text {
                objectName: "settingsTitle"
                text: "设置"
                color: settingsPanel.tokens.ink
                font.pixelSize: settingsPanel.tokens.sizeTitle + 8
                font.weight: Font.DemiBold
                font.family: settingsPanel.tokens.fontUi
                Layout.fillWidth: true
                renderType: Text.NativeRendering
            }

            SettingsCard {
                objectName: "settingsApiCard"

                content: [
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            objectName: "settingsApiTitle"
                            text: "AI 接口"
                            color: settingsPanel.tokens.ink
                            font.pixelSize: settingsPanel.tokens.sizeTitle + 2
                            font.weight: Font.DemiBold
                            font.family: settingsPanel.tokens.fontUi
                            Layout.fillWidth: true
                            renderType: Text.NativeRendering
                        }

                        ToolPill {
                            objectName: "settingsApiToggle"
                            text: settingsPanel.apiExpanded ? "收起" : "展开"
                            widthHint: 54
                            uiFontFamily: settingsPanel.uiFontFamily
                            uiFontSize: settingsPanel.uiFontSize
                            onClicked: settingsPanel.toggleApiSection()
                        }
                    },

                    ColumnLayout {
                        visible: settingsPanel.apiExpanded
                        Layout.fillWidth: true
                        spacing: 8

                        SettingsField {
                            id: apiUrlField
                            objectName: "apiUrlField"
                            Layout.fillWidth: true
                            placeholderText: "API 地址"
                        }

                        SettingsField {
                            id: apiKeyField
                            objectName: "apiKeyField"
                            Layout.fillWidth: true
                            placeholderText: "API Key"
                            echoMode: TextInput.Password
                        }

                        SettingsField {
                            id: modelField
                            objectName: "modelField"
                            Layout.fillWidth: true
                            placeholderText: "模型名称"
                        }
                    }
                ]
            }

            SettingsCard {
                objectName: "settingsAppearanceCard"

                content: [
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            objectName: "settingsAppearanceTitle"
                            text: "界面"
                            color: settingsPanel.tokens.ink
                            font.pixelSize: settingsPanel.tokens.sizeTitle + 2
                            font.weight: Font.DemiBold
                            font.family: settingsPanel.tokens.fontUi
                            Layout.fillWidth: true
                            renderType: Text.NativeRendering
                        }

                        ToolPill {
                            objectName: "settingsAppearanceToggle"
                            text: settingsPanel.appearanceExpanded ? "收起" : "展开"
                            widthHint: 54
                            uiFontFamily: settingsPanel.uiFontFamily
                            uiFontSize: settingsPanel.uiFontSize
                            onClicked: settingsPanel.toggleAppearanceSection()
                        }
                    },

                    GridLayout {
                        visible: settingsPanel.appearanceExpanded
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 10
                        rowSpacing: 8

                        Text {
                            text: "界面字体"
                            color: settingsPanel.tokens.muted
                            font.pixelSize: settingsPanel.tokens.sizeBody
                            font.family: settingsPanel.tokens.fontUi
                            renderType: Text.NativeRendering
                        }

                        ComboBox {
                            id: fontFamilyCombo
                            objectName: "settingsFontFamilyCombo"
                            Layout.fillWidth: true
                            model: settingsPanel.fontPickerModel
                            font.pixelSize: settingsPanel.tokens.sizeBody + 1
                            font.family: settingsPanel.tokens.fontUi
                            Component.onCompleted: settingsPanel.syncFontControls()
                        }

                        Text {
                            text: "字号"
                            color: settingsPanel.tokens.muted
                            font.pixelSize: settingsPanel.tokens.sizeBody
                            font.family: settingsPanel.tokens.fontUi
                            renderType: Text.NativeRendering
                        }

                        SpinBox {
                            id: fontSizeBox
                            objectName: "settingsFontSizeField"
                            Layout.fillWidth: true
                            from: 10
                            to: 18
                            value: settingsPanel.uiFontSize
                            editable: true
                            font.pixelSize: settingsPanel.tokens.sizeBody + 1
                            font.family: settingsPanel.tokens.fontUi
                        }
                    }
                ]
            }

            SettingsCard {
                objectName: "settingsDataCard"

                content: [
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            objectName: "settingsDataTitle"
                            text: "便签数据"
                            color: settingsPanel.tokens.ink
                            font.pixelSize: settingsPanel.tokens.sizeTitle + 2
                            font.weight: Font.DemiBold
                            font.family: settingsPanel.tokens.fontUi
                            Layout.fillWidth: true
                            renderType: Text.NativeRendering
                        }

                        ToolPill {
                            objectName: "settingsDataToggle"
                            text: settingsPanel.dataExpanded ? "收起" : "展开"
                            widthHint: 54
                            uiFontFamily: settingsPanel.uiFontFamily
                            uiFontSize: settingsPanel.uiFontSize
                            onClicked: settingsPanel.toggleDataSection()
                        }
                    },

                    ColumnLayout {
                        visible: settingsPanel.dataExpanded
                        Layout.fillWidth: true
                        spacing: 10

                        Text {
                            objectName: "settingsDataDescription"
                            text: "这些操作只作用于便签数据，不影响项目树。"
                            color: settingsPanel.tokens.muted
                            font.pixelSize: settingsPanel.tokens.sizeBody + 1
                            font.family: settingsPanel.tokens.fontUi
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                            renderType: Text.NativeRendering
                        }

                        GridLayout {
                            columns: settingsPanel.width < 520 ? 1 : 2
                            columnSpacing: 10
                            rowSpacing: 10
                            Layout.fillWidth: true

                            ToolPill {
                                objectName: "exportStickiesJsonButton"
                                text: "导出便签 JSON"
                                widthHint: 140
                                uiFontFamily: settingsPanel.uiFontFamily
                                uiFontSize: settingsPanel.uiFontSize
                                Layout.fillWidth: true
                                onClicked: settingsPanel.exportJsonRequested()
                            }

                            ToolPill {
                                objectName: "importStickiesJsonButton"
                                text: "导入便签 JSON"
                                widthHint: 140
                                uiFontFamily: settingsPanel.uiFontFamily
                                uiFontSize: settingsPanel.uiFontSize
                                Layout.fillWidth: true
                                onClicked: settingsPanel.importJsonRequested()
                            }

                            ToolPill {
                                objectName: "exportStickiesMarkdownButton"
                                text: "导出便签 Markdown"
                                widthHint: 160
                                uiFontFamily: settingsPanel.uiFontFamily
                                uiFontSize: settingsPanel.uiFontSize
                                Layout.fillWidth: true
                                onClicked: settingsPanel.exportMarkdownRequested()
                            }

                            ToolPill {
                                objectName: "clearCompletedStickiesButton"
                                text: "清理已完成便签"
                                widthHint: 150
                                uiFontFamily: settingsPanel.uiFontFamily
                                uiFontSize: settingsPanel.uiFontSize
                                danger: true
                                Layout.fillWidth: true
                                onClicked: settingsPanel.clearCompletedRequested()
                            }

                            ToolPill {
                                objectName: "clearCurrentStickiesButton"
                                text: "清空当前便签阶段"
                                widthHint: 150
                                uiFontFamily: settingsPanel.uiFontFamily
                                uiFontSize: settingsPanel.uiFontSize
                                danger: true
                                Layout.fillWidth: true
                                onClicked: settingsPanel.clearCurrentRequested()
                            }

                            ToolPill {
                                objectName: "clearAllStickiesButton"
                                text: settingsPanel.clearAllArmed ? "确认清空全部便签" : "清空全部便签"
                                widthHint: 150
                                uiFontFamily: settingsPanel.uiFontFamily
                                uiFontSize: settingsPanel.uiFontSize
                                danger: true
                                Layout.fillWidth: true
                                onClicked: {
                                    if (settingsPanel.clearAllArmed) {
                                        settingsPanel.clearAllRequested()
                                        settingsPanel.clearAllArmed = false
                                    } else {
                                        settingsPanel.clearAllArmed = true
                                    }
                                }
                            }
                        }
                    }
                ]
            }

            ToolPill {
                objectName: "settingsSaveButton"
                text: "保存设置"
                widthHint: 112
                primary: true
                uiFontFamily: settingsPanel.uiFontFamily
                uiFontSize: settingsPanel.uiFontSize
                Layout.alignment: Qt.AlignRight
                onClicked: settingsPanel.saveRequested(apiUrlField.text, apiKeyField.text, modelField.text,
                                                        fontFamilyCombo.currentText, fontSizeBox.value)
            }
        }
    }
}
