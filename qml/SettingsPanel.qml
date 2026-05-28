import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root

    property alias apiUrl: apiUrlField.text
    property alias apiKey: apiKeyField.text
    property alias modelName: modelNameField.text
    property bool clearAllArmed: false
    property bool apiExpanded: true
    property bool dataExpanded: false
    property bool inputActiveFocus: apiUrlField.activeFocus || apiKeyField.activeFocus || modelNameField.activeFocus

    signal saveRequested(string url, string key, string model)
    signal clearCompletedRequested()
    signal clearCompletedAllRequested()
    signal clearCurrentRequested()
    signal clearAllRequested()
    signal exportJsonRequested()
    signal importJsonRequested()
    signal exportMarkdownRequested()

    spacing: 14

    function releaseInputFocus() {
        apiUrlField.focus = false
        apiKeyField.focus = false
        modelNameField.focus = false
    }

    function resetTextViews() {
        apiUrlField.cursorPosition = 0
        apiKeyField.cursorPosition = 0
        modelNameField.cursorPosition = 0
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

    onVisibleChanged: if (visible) Qt.callLater(resetTextViews)

    ScrollView {
        id: scroll
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: scroll.availableWidth
            spacing: 14

            Text {
                Layout.fillWidth: true
                text: "设置"
                color: "#071426"
                font.pixelSize: 23
                font.weight: Font.Bold
                font.family: "Microsoft YaHei UI"
                renderType: Text.NativeRendering
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: apiColumn.implicitHeight + 28
                radius: 18
                color: "#88ffffff"

                ColumnLayout {
                    id: apiColumn
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            Layout.fillWidth: true
                            text: "AI 接口"
                            color: "#071426"
                            font.pixelSize: 15
                            font.weight: Font.Bold
                            font.family: "Microsoft YaHei UI"
                            renderType: Text.NativeRendering
                        }

                        Text {
                            text: root.apiExpanded ? "收起" : "展开"
                            color: "#2d68c7"
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            font.family: "Microsoft YaHei UI"
                            renderType: Text.NativeRendering
                        }

                        TapHandler {
                            onTapped: root.toggleApiSection()
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: root.apiExpanded
                        text: "兼容 OpenAI 的接口配置。保存后，智能摘要会使用这里的地址、密钥和模型。"
                        color: "#607086"
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        font.family: "Microsoft YaHei UI"
                        renderType: Text.NativeRendering
                    }

                    TextField {
                        id: apiUrlField
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        visible: root.apiExpanded
                        placeholderText: "API URL"
                        leftPadding: 16
                        rightPadding: 16
                        font.pixelSize: 13
                        font.family: "Microsoft YaHei UI"
                        renderType: Text.NativeRendering
                        background: Rectangle {
                            radius: 14
                            color: "#99ffffff"
                            border.color: apiUrlField.activeFocus ? "#2d68c7" : "transparent"
                            border.width: 1
                        }
                    }

                    TextField {
                        id: apiKeyField
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        visible: root.apiExpanded
                        placeholderText: "API Key"
                        echoMode: TextInput.PasswordEchoOnEdit
                        leftPadding: 16
                        rightPadding: 16
                        font.pixelSize: 13
                        font.family: "Microsoft YaHei UI"
                        renderType: Text.NativeRendering
                        background: Rectangle {
                            radius: 14
                            color: "#99ffffff"
                            border.color: apiKeyField.activeFocus ? "#2d68c7" : "transparent"
                            border.width: 1
                        }
                    }

                    TextField {
                        id: modelNameField
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        visible: root.apiExpanded
                        placeholderText: "Model"
                        leftPadding: 16
                        rightPadding: 16
                        font.pixelSize: 13
                        font.family: "Microsoft YaHei UI"
                        renderType: Text.NativeRendering
                        background: Rectangle {
                            radius: 14
                            color: "#99ffffff"
                            border.color: modelNameField.activeFocus ? "#2d68c7" : "transparent"
                            border.width: 1
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: dataColumn.implicitHeight + 40
                radius: 18
                color: "#88ffffff"

                ColumnLayout {
                    id: dataColumn
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 14

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            Layout.fillWidth: true
                            text: "数据管理"
                            color: "#071426"
                            font.pixelSize: 15
                            font.weight: Font.Bold
                            font.family: "Microsoft YaHei UI"
                            renderType: Text.NativeRendering
                        }

                        Text {
                            text: root.dataExpanded ? "收起" : "展开"
                            color: "#2d68c7"
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            font.family: "Microsoft YaHei UI"
                            renderType: Text.NativeRendering
                        }

                        TapHandler {
                            onTapped: root.toggleDataSection()
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: root.dataExpanded
                        text: "清理会直接改数据；备份和恢复会打开文件选择窗口。"
                        color: "#607086"
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        font.family: "Microsoft YaHei UI"
                        renderType: Text.NativeRendering
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: root.dataExpanded
                        text: "清理"
                        color: "#40577a"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        font.family: "Microsoft YaHei UI"
                        renderType: Text.NativeRendering
                    }

                    Flow {
                        Layout.fillWidth: true
                        Layout.preferredHeight: implicitHeight
                        visible: root.dataExpanded
                        spacing: 12

                        ToolPill {
                            text: "删当前已完成"
                            widthHint: 104
                            danger: true
                            onClicked: root.clearCompletedRequested()
                        }

                        ToolPill {
                            text: "删全部已完成"
                            widthHint: 104
                            danger: true
                            onClicked: root.clearCompletedAllRequested()
                        }

                        ToolPill {
                            text: "清空当前"
                            widthHint: 84
                            danger: true
                            onClicked: root.clearCurrentRequested()
                        }

                        ToolPill {
                            text: root.clearAllArmed ? "确认全删" : "全删"
                            widthHint: 76
                            danger: true
                            onClicked: {
                                if (root.clearAllArmed) {
                                    root.clearAllRequested()
                                    root.clearAllArmed = false
                                } else {
                                    root.clearAllArmed = true
                                }
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: root.dataExpanded
                        text: "备份 / 恢复"
                        color: "#40577a"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        font.family: "Microsoft YaHei UI"
                        renderType: Text.NativeRendering
                    }

                    Flow {
                        Layout.fillWidth: true
                        Layout.preferredHeight: implicitHeight
                        visible: root.dataExpanded
                        spacing: 12

                        ToolPill {
                            text: "导出 JSON"
                            widthHint: 92
                            onClicked: root.exportJsonRequested()
                        }

                        ToolPill {
                            text: "导入 JSON"
                            widthHint: 92
                            onClicked: root.importJsonRequested()
                        }

                        ToolPill {
                            text: "导出 MD"
                            widthHint: 82
                            onClicked: root.exportMarkdownRequested()
                        }
                    }
                }
            }
        }
    }

    Button {
        id: saveSettings
        Layout.fillWidth: true
        Layout.preferredHeight: 44
        hoverEnabled: true
        onClicked: root.saveRequested(apiUrlField.text, apiKeyField.text, modelNameField.text)
        contentItem: Text {
            text: "保存设置"
            color: "#ffffff"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 14
            font.weight: Font.Bold
            font.family: "Microsoft YaHei UI"
            renderType: Text.NativeRendering
        }
        background: Rectangle {
            radius: 16
            color: saveSettings.hovered ? "#245db6" : "#2d68c7"
            Behavior on color { ColorAnimation { duration: 130 } }
        }
    }
}
