import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: settingsPanel
    objectName: "settingsPanelSurface"

    readonly property ChronoTokens tokens: ChronoTokens {}

    property alias apiUrl: apiUrlField.text
    property alias apiKey: apiKeyField.text
    property alias modelName: modelField.text
    property bool clearAllArmed: false
    property bool apiExpanded: true
    property bool dataExpanded: true
    property bool inputActiveFocus: apiUrlField.activeFocus || apiKeyField.activeFocus || modelField.activeFocus
    property color surfaceColor: tokens.paper

    signal saveRequested(string url, string key, string model)
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

    function releaseInputFocus() {
        apiUrlField.focus = false
        apiKeyField.focus = false
        modelField.focus = false
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

    function dataSectionExpanded() {
        return dataExpanded
    }

    function toggleApiSection() {
        apiExpanded = !apiExpanded
    }

    function toggleDataSection() {
        dataExpanded = !dataExpanded
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
                font.pixelSize: 22
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }

            Rectangle {
                objectName: "settingsApiCard"
                Layout.fillWidth: true
                color: "#bffffef7"
                radius: settingsPanel.tokens.radiusMd
                border.color: "#2288c57f"
                implicitHeight: apiHeader.height + (apiSection.visible ? apiSection.implicitHeight + 14 : 0) + 18

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    RowLayout {
                        id: apiHeader
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            objectName: "settingsApiTitle"
                            text: "AI 接口"
                            color: settingsPanel.tokens.ink
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                            Layout.fillWidth: true
                        }

                        ToolPill {
                            objectName: "settingsApiToggle"
                            text: settingsPanel.apiExpanded ? "收起" : "展开"
                            widthHint: 54
                            onClicked: settingsPanel.toggleApiSection()
                        }
                    }

                    ColumnLayout {
                        id: apiSection
                        visible: settingsPanel.apiExpanded
                        Layout.fillWidth: true
                        spacing: 8

                        TextField {
                            id: apiUrlField
                            objectName: "apiUrlField"
                            Layout.fillWidth: true
                            placeholderText: "API 地址"
                            background: Rectangle {
                                radius: settingsPanel.tokens.radiusSm
                                color: "#ccfffef7"
                                border.width: 1
                                border.color: apiUrlField.activeFocus ? settingsPanel.tokens.accentMint : settingsPanel.tokens.lineSoft
                            }
                        }

                        TextField {
                            id: apiKeyField
                            objectName: "apiKeyField"
                            Layout.fillWidth: true
                            placeholderText: "API Key"
                            echoMode: TextInput.Password
                            background: Rectangle {
                                radius: settingsPanel.tokens.radiusSm
                                color: "#ccfffef7"
                                border.width: 1
                                border.color: apiKeyField.activeFocus ? settingsPanel.tokens.accentMint : settingsPanel.tokens.lineSoft
                            }
                        }

                        TextField {
                            id: modelField
                            objectName: "modelField"
                            Layout.fillWidth: true
                            placeholderText: "模型名称"
                            background: Rectangle {
                                radius: settingsPanel.tokens.radiusSm
                                color: "#ccfffef7"
                                border.width: 1
                                border.color: modelField.activeFocus ? settingsPanel.tokens.accentMint : settingsPanel.tokens.lineSoft
                            }
                        }

                        ToolPill {
                            objectName: "settingsSaveButton"
                            text: "保存 AI 设置"
                            widthHint: 116
                            primary: true
                            Layout.alignment: Qt.AlignRight
                            onClicked: settingsPanel.saveRequested(apiUrlField.text, apiKeyField.text, modelField.text)
                        }
                    }
                }
            }

            Rectangle {
                objectName: "settingsDataCard"
                Layout.fillWidth: true
                color: "#bffffef7"
                radius: settingsPanel.tokens.radiusMd
                border.color: "#2288c57f"
                implicitHeight: dataHeader.height + (dataSection.visible ? dataSection.implicitHeight + 14 : 0) + 18

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    RowLayout {
                        id: dataHeader
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            objectName: "settingsDataTitle"
                            text: "便签数据"
                            color: settingsPanel.tokens.ink
                            font.pixelSize: 16
                            font.weight: Font.DemiBold
                            Layout.fillWidth: true
                        }

                        ToolPill {
                            objectName: "settingsDataToggle"
                            text: settingsPanel.dataExpanded ? "收起" : "展开"
                            widthHint: 54
                            onClicked: settingsPanel.toggleDataSection()
                        }
                    }

                    ColumnLayout {
                        id: dataSection
                        visible: settingsPanel.dataExpanded
                        Layout.fillWidth: true
                        spacing: 10

                        Text {
                            objectName: "settingsDataDescription"
                            text: "这些操作只作用于便签数据，不影响项目树。"
                            color: settingsPanel.tokens.muted
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
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
                                Layout.fillWidth: true
                                onClicked: settingsPanel.exportJsonRequested()
                            }

                            ToolPill {
                                objectName: "importStickiesJsonButton"
                                text: "导入便签 JSON"
                                widthHint: 140
                                Layout.fillWidth: true
                                onClicked: settingsPanel.importJsonRequested()
                            }

                            ToolPill {
                                objectName: "exportStickiesMarkdownButton"
                                text: "导出便签 Markdown"
                                widthHint: 160
                                Layout.fillWidth: true
                                onClicked: settingsPanel.exportMarkdownRequested()
                            }

                            ToolPill {
                                objectName: "clearCompletedStickiesButton"
                                text: "清理已完成便签"
                                widthHint: 150
                                danger: true
                                Layout.fillWidth: true
                                onClicked: settingsPanel.clearCompletedRequested()
                            }

                            ToolPill {
                                objectName: "clearCurrentStickiesButton"
                                text: "清空当前便签阶段"
                                widthHint: 150
                                danger: true
                                Layout.fillWidth: true
                                onClicked: settingsPanel.clearCurrentRequested()
                            }

                            ToolPill {
                                objectName: "clearAllStickiesButton"
                                text: settingsPanel.clearAllArmed ? "确认清空全部便签" : "清空全部便签"
                                widthHint: 150
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
                }
            }
        }
    }
}
