pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: settingsPanel
    objectName: "settingsPanelSurface"

    readonly property ChronoTokens fallbackTokens: ChronoTokens {
        fontUi: settingsPanel.uiFontFamily
        baseFontSize: settingsPanel.uiFontSize
        reduceMotion: settingsPanel.reduceMotion
    }
    readonly property var tokens: settingsPanel.theme ? settingsPanel.theme : settingsPanel.fallbackTokens

    property var theme: null
    property alias apiUrl: apiUrlField.text
    property alias newApiKey: apiKeyDraftField.text
    property alias modelName: modelField.text
    property bool hasApiKey: false
    property bool allowLocalHttp: false
    property bool reduceMotion: false
    property string dataDirectory: ""
    property string uiFontFamily: theme ? theme.fontUi : "Microsoft YaHei UI"
    property int uiFontSize: theme ? theme.baseFontSize : 14
    property var uiFontFamilies: ["Microsoft YaHei UI"]
    property var backupEntries: []
    property string recoveryStatus: ""
    property string recoveryError: ""
    property bool recoveryBusy: false
    property bool clearAllArmed: false
    property bool apiExpanded: true
    property bool appearanceExpanded: true
    property bool recoveryExpanded: true
    property bool maintenanceExpanded: false
    readonly property bool inputActiveFocus: apiUrlField.activeFocus || apiKeyDraftField.activeFocus ||
                                             modelField.activeFocus || fontFamilyCombo.activeFocus ||
                                             fontSizeBox.activeFocus
    property color surfaceColor: tokens.drawerPaper
    readonly property var fontPickerModel: uiFontFamilies && uiFontFamilies.length > 0
                                           ? uiFontFamilies
                                           : [uiFontFamily.length > 0 ? uiFontFamily : "Microsoft YaHei UI"]
    readonly property bool hasBackups: backupEntries && backupEntries.length > 0

    signal saveRequested(string url, string newKey, string model, bool allowHttp,
                         bool reduceAnimations, string fontFamily, int fontSize)
    signal clearApiKeyRequested()
    signal createBackupRequested()
    signal exportWorkspaceRequested()
    signal importWorkspaceRequested()
    signal restoreBackupRequested(var backupUrl)
    signal exportDiagnosticsRequested()
    signal openDataDirectoryRequested()
    signal refreshBackupsRequested()
    signal clearCompletedRequested()
    signal clearCompletedAllRequested()
    signal clearCurrentRequested()
    signal clearAllRequested()
    signal exportJsonRequested()
    signal importJsonRequested()
    signal exportMarkdownRequested()

    color: surfaceColor
    radius: tokens.radiusLg
    border.color: tokens.border
    border.width: 1
    Accessible.role: Accessible.Pane
    Accessible.name: "设置与恢复中心"
    Accessible.description: "配置 AI、界面、备份恢复和便签维护"

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
        apiKeyDraftField.focus = false
        modelField.focus = false
        fontFamilyCombo.focus = false
        fontSizeBox.focus = false
    }

    function focusInitial() {
        apiUrlField.forceActiveFocus(Qt.TabFocusReason)
    }

    function resetTextViews() {
        releaseInputFocus()
        apiUrlField.cursorPosition = 0
        apiKeyDraftField.cursorPosition = 0
        modelField.cursorPosition = 0
    }

    function clearApiKeyDraft() {
        apiKeyDraftField.clear()
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
        return recoveryExpanded
    }

    function toggleApiSection() {
        apiExpanded = !apiExpanded
    }

    function toggleAppearanceSection() {
        appearanceExpanded = !appearanceExpanded
    }

    function toggleDataSection() {
        recoveryExpanded = !recoveryExpanded
    }

    function selectedBackupUrl() {
        if (!hasBackups || backupPicker.currentIndex < 0)
            return ""
        const current = backupEntries[backupPicker.currentIndex]
        return current && current.url !== undefined ? current.url : ""
    }

    onUiFontFamilyChanged: Qt.callLater(syncFontControls)
    onUiFontSizeChanged: Qt.callLater(syncFontControls)
    onUiFontFamiliesChanged: Qt.callLater(syncFontControls)
    Component.onCompleted: syncFontControls()

    component SettingsField: TextField {
        id: field
        selectByMouse: true
        activeFocusOnTab: true
        font.pixelSize: settingsPanel.tokens.sizeBody
        font.family: settingsPanel.tokens.fontUi
        color: settingsPanel.tokens.ink
        placeholderTextColor: settingsPanel.tokens.textDisabled
        selectionColor: settingsPanel.tokens.accentSoft
        selectedTextColor: settingsPanel.tokens.ink
        Accessible.role: Accessible.EditableText
        background: Rectangle {
            radius: settingsPanel.tokens.radiusSm
            color: field.activeFocus ? settingsPanel.tokens.surfaceHover : settingsPanel.tokens.surface
            border.width: field.activeFocus ? 2 : 1
            border.color: field.activeFocus ? settingsPanel.tokens.focusRing : settingsPanel.tokens.border
            Behavior on color {
                ColorAnimation { duration: settingsPanel.tokens.motionFast; easing.type: Easing.OutCubic }
            }
            Behavior on border.color {
                ColorAnimation { duration: settingsPanel.tokens.motionFast; easing.type: Easing.OutCubic }
            }
        }
    }

    component SettingsCard: Rectangle {
        default property alias content: slot.data

        Layout.fillWidth: true
        color: settingsPanel.tokens.drawerCard
        radius: settingsPanel.tokens.radiusMd
        border.color: settingsPanel.tokens.borderSubtle
        border.width: 1
        implicitHeight: slot.implicitHeight + settingsPanel.tokens.space4 * 2

        ColumnLayout {
            id: slot
            anchors.fill: parent
            anchors.margins: settingsPanel.tokens.space4
            spacing: settingsPanel.tokens.space3
        }
    }

    component SectionHeading: RowLayout {
        required property string title
        required property bool expanded
        signal toggleRequested()

        Layout.fillWidth: true
        spacing: settingsPanel.tokens.space2

        Text {
            Layout.fillWidth: true
            text: parent.title
            color: settingsPanel.tokens.ink
            font.pixelSize: settingsPanel.tokens.sizeHeading
            font.weight: Font.DemiBold
            font.family: settingsPanel.tokens.fontUi
            renderType: Text.NativeRendering
            Accessible.role: Accessible.Heading
            Accessible.name: text
        }

        ToolPill {
            text: parent.expanded ? "收起" : "展开"
            widthHint: 60
            theme: settingsPanel.tokens
            accessibleDescription: (parent.expanded ? "收起" : "展开") + parent.title + "设置"
            onClicked: parent.toggleRequested()
        }
    }

    ScrollView {
        id: settingsScroll
        anchors.fill: parent
        anchors.margins: settingsPanel.tokens.space4
        clip: true
        activeFocusOnTab: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AsNeeded
        Accessible.role: Accessible.ScrollBar
        Accessible.name: "设置内容"

        ColumnLayout {
            width: settingsScroll.availableWidth
            spacing: settingsPanel.tokens.space3

            Text {
                objectName: "settingsTitle"
                text: "设置"
                color: settingsPanel.tokens.ink
                font.pixelSize: settingsPanel.tokens.sizeDisplay
                font.weight: Font.Bold
                font.family: settingsPanel.tokens.fontUi
                Layout.fillWidth: true
                renderType: Text.NativeRendering
                Accessible.role: Accessible.Heading
                Accessible.name: text
            }

            SettingsCard {
                objectName: "settingsApiCard"

                SectionHeading {
                    id: apiHeading
                    title: "AI 接口"
                    expanded: settingsPanel.apiExpanded
                    onToggleRequested: settingsPanel.toggleApiSection()
                }

                ColumnLayout {
                    visible: settingsPanel.apiExpanded
                    Layout.fillWidth: true
                    spacing: settingsPanel.tokens.space2

                    Rectangle {
                        objectName: "apiCredentialStatus"
                        Layout.fillWidth: true
                        implicitHeight: credentialStatusText.implicitHeight + settingsPanel.tokens.space3 * 2
                        radius: settingsPanel.tokens.radiusSm
                        color: settingsPanel.hasApiKey ? settingsPanel.tokens.completedSurface
                                                       : settingsPanel.tokens.surfaceMuted
                        border.color: settingsPanel.hasApiKey ? settingsPanel.tokens.projectAccent
                                                              : settingsPanel.tokens.border

                        Text {
                            id: credentialStatusText
                            anchors.fill: parent
                            anchors.margins: settingsPanel.tokens.space3
                            text: settingsPanel.hasApiKey
                                  ? "API Key 已安全保存在 Windows 凭据管理器"
                                  : "尚未配置 API Key"
                            color: settingsPanel.hasApiKey ? settingsPanel.tokens.projectAccent
                                                          : settingsPanel.tokens.textSecondary
                            font.family: settingsPanel.tokens.fontUi
                            font.pixelSize: settingsPanel.tokens.sizeBody
                            wrapMode: Text.WordWrap
                            renderType: Text.NativeRendering
                        }
                    }

                    SettingsField {
                        id: apiUrlField
                        objectName: "apiUrlField"
                        Layout.fillWidth: true
                        placeholderText: "HTTPS API 地址"
                        Accessible.name: "API 地址"
                        Accessible.description: "默认仅允许 HTTPS；本机 HTTP 需单独开启"
                    }

                    SettingsField {
                        id: apiKeyDraftField
                        objectName: "apiKeyField"
                        Layout.fillWidth: true
                        placeholderText: settingsPanel.hasApiKey
                                         ? "输入新 Key 以替换；留空保持不变"
                                         : "输入 API Key"
                        echoMode: TextInput.Password
                        Accessible.name: "新的 API Key"
                        Accessible.description: "只用于替换凭据；已有 Key 不会回显到界面"
                    }

                    SettingsField {
                        id: modelField
                        objectName: "modelField"
                        Layout.fillWidth: true
                        placeholderText: "模型名称"
                        Accessible.name: "模型名称"
                        Accessible.description: "输入 OpenAI 兼容接口的模型标识"
                    }

                    CheckBox {
                        id: localHttpCheck
                        objectName: "allowLocalHttpCheck"
                        text: "允许 localhost 使用 HTTP"
                        checked: settingsPanel.allowLocalHttp
                        activeFocusOnTab: true
                        font.family: settingsPanel.tokens.fontUi
                        font.pixelSize: settingsPanel.tokens.sizeBody
                        Accessible.name: text
                        Accessible.description: "仅为本机开发服务放宽 HTTPS 限制"
                        onToggled: settingsPanel.allowLocalHttp = checked
                    }

                    ToolPill {
                        objectName: "clearApiKeyButton"
                        visible: settingsPanel.hasApiKey
                        text: "删除已保存的 API Key"
                        danger: true
                        widthHint: 180
                        theme: settingsPanel.tokens
                        accessibleDescription: "从 Windows 凭据管理器删除 API Key"
                        onClicked: settingsPanel.clearApiKeyRequested()
                    }
                }
            }

            SettingsCard {
                objectName: "settingsAppearanceCard"

                SectionHeading {
                    id: appearanceHeading
                    title: "界面"
                    expanded: settingsPanel.appearanceExpanded
                    onToggleRequested: settingsPanel.toggleAppearanceSection()
                }

                GridLayout {
                    visible: settingsPanel.appearanceExpanded
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: settingsPanel.tokens.space3
                    rowSpacing: settingsPanel.tokens.space2

                    Text {
                        text: "界面字体"
                        color: settingsPanel.tokens.textSecondary
                        font.pixelSize: settingsPanel.tokens.sizeBody
                        font.family: settingsPanel.tokens.fontUi
                        renderType: Text.NativeRendering
                    }

                    ComboBox {
                        id: fontFamilyCombo
                        objectName: "settingsFontFamilyCombo"
                        Layout.fillWidth: true
                        model: settingsPanel.fontPickerModel
                        activeFocusOnTab: true
                        font.pixelSize: settingsPanel.tokens.sizeBody
                        font.family: settingsPanel.tokens.fontUi
                        Accessible.name: "界面字体"
                        Accessible.description: "选择 ChronoNotes 使用的字体"
                        Component.onCompleted: settingsPanel.syncFontControls()
                    }

                    Text {
                        text: "字号"
                        color: settingsPanel.tokens.textSecondary
                        font.pixelSize: settingsPanel.tokens.sizeBody
                        font.family: settingsPanel.tokens.fontUi
                        renderType: Text.NativeRendering
                    }

                    SpinBox {
                        id: fontSizeBox
                        objectName: "settingsFontSizeField"
                        Layout.fillWidth: true
                        from: 12
                        to: 20
                        value: settingsPanel.uiFontSize
                        editable: true
                        activeFocusOnTab: true
                        font.pixelSize: settingsPanel.tokens.sizeBody
                        font.family: settingsPanel.tokens.fontUi
                        Accessible.name: "界面字号"
                        Accessible.description: "设置界面基础字号，范围 12 到 20"
                    }

                    CheckBox {
                        id: reduceMotionCheck
                        objectName: "reduceMotionCheck"
                        Layout.columnSpan: 2
                        text: "减少界面动画"
                        checked: settingsPanel.reduceMotion
                        activeFocusOnTab: true
                        font.family: settingsPanel.tokens.fontUi
                        font.pixelSize: settingsPanel.tokens.sizeBody
                        Accessible.name: text
                        Accessible.description: "关闭抽屉和控件的过渡动画"
                        onToggled: settingsPanel.reduceMotion = checked
                    }
                }
            }

            SettingsCard {
                objectName: "settingsRecoveryCard"

                SectionHeading {
                    id: recoveryHeading
                    title: "恢复中心"
                    expanded: settingsPanel.recoveryExpanded
                    onToggleRequested: settingsPanel.toggleDataSection()
                }

                ColumnLayout {
                    visible: settingsPanel.recoveryExpanded
                    Layout.fillWidth: true
                    spacing: settingsPanel.tokens.space2

                    Text {
                        objectName: "settingsDataTitle"
                        visible: false
                        text: "数据与恢复"
                    }

                    Text {
                        objectName: "settingsDataDescription"
                        Layout.fillWidth: true
                        text: "工作区备份包含便签、项目树、摘要历史和非敏感偏好；不会包含 API Key。"
                        color: settingsPanel.tokens.textSecondary
                        font.pixelSize: settingsPanel.tokens.sizeBody
                        font.family: settingsPanel.tokens.fontUi
                        wrapMode: Text.WordWrap
                        renderType: Text.NativeRendering
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: dataLocationText.implicitHeight + settingsPanel.tokens.space3 * 2
                        radius: settingsPanel.tokens.radiusSm
                        color: settingsPanel.tokens.surfaceMuted
                        border.color: settingsPanel.tokens.borderSubtle

                        Text {
                            id: dataLocationText
                            anchors.fill: parent
                            anchors.margins: settingsPanel.tokens.space3
                            text: "数据位置\n" + (settingsPanel.dataDirectory.length > 0
                                               ? settingsPanel.dataDirectory : "暂不可用")
                            color: settingsPanel.tokens.ink
                            font.family: settingsPanel.tokens.fontUi
                            font.pixelSize: settingsPanel.tokens.sizeMeta
                            wrapMode: Text.WrapAnywhere
                            renderType: Text.NativeRendering
                            Accessible.role: Accessible.StaticText
                            Accessible.name: "数据位置"
                            Accessible.description: settingsPanel.dataDirectory
                        }
                    }

                    ToolPill {
                        objectName: "openDataDirectoryButton"
                        Layout.fillWidth: true
                        text: "打开数据目录"
                        enabled: settingsPanel.dataDirectory.length > 0
                        theme: settingsPanel.tokens
                        accessibleDescription: "在文件管理器中打开 ChronoNotes 数据目录"
                        onClicked: settingsPanel.openDataDirectoryRequested()
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: settingsPanel.width < 420 ? 1 : 2
                        columnSpacing: settingsPanel.tokens.space2
                        rowSpacing: settingsPanel.tokens.space2

                        ToolPill {
                            objectName: "createWorkspaceBackupButton"
                            Layout.fillWidth: true
                            text: "创建工作区备份"
                            primary: true
                            enabled: !settingsPanel.recoveryBusy
                            theme: settingsPanel.tokens
                            accessibleDescription: "立即创建 .chrononotes 工作区备份"
                            onClicked: settingsPanel.createBackupRequested()
                        }

                        ToolPill {
                            objectName: "importWorkspaceButton"
                            Layout.fillWidth: true
                            text: "导入工作区"
                            enabled: !settingsPanel.recoveryBusy
                            theme: settingsPanel.tokens
                            accessibleDescription: "选择并预览 .chrononotes 工作区备份"
                            onClicked: settingsPanel.importWorkspaceRequested()
                        }

                        ToolPill {
                            objectName: "exportWorkspaceButton"
                            Layout.fillWidth: true
                            text: "导出工作区"
                            enabled: !settingsPanel.recoveryBusy
                            theme: settingsPanel.tokens
                            accessibleDescription: "将当前工作区导出为 .chrononotes 文件"
                            onClicked: settingsPanel.exportWorkspaceRequested()
                        }

                        ToolPill {
                            objectName: "exportDiagnosticsButton"
                            Layout.fillWidth: true
                            text: "导出脱敏诊断"
                            enabled: !settingsPanel.recoveryBusy
                            theme: settingsPanel.tokens
                            accessibleDescription: "导出不含便签正文、AI 内容和凭据的诊断文件"
                            onClicked: settingsPanel.exportDiagnosticsRequested()
                        }
                    }

                    ComboBox {
                        id: backupPicker
                        objectName: "backupPicker"
                        Layout.fillWidth: true
                        model: settingsPanel.backupEntries
                        textRole: "name"
                        valueRole: "url"
                        enabled: settingsPanel.hasBackups && !settingsPanel.recoveryBusy
                        activeFocusOnTab: true
                        displayText: settingsPanel.hasBackups
                                     ? currentText
                                     : "暂无可恢复的自动备份"
                        Accessible.name: "可恢复备份"
                        Accessible.description: settingsPanel.hasBackups
                                                ? "选择要恢复的自动备份"
                                                : "当前没有可恢复备份"
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: settingsPanel.tokens.space2

                        ToolPill {
                            objectName: "restoreBackupButton"
                            Layout.fillWidth: true
                            text: "恢复所选备份"
                            danger: true
                            enabled: settingsPanel.hasBackups && !settingsPanel.recoveryBusy
                            theme: settingsPanel.tokens
                            accessibleDescription: "恢复所选备份；恢复前会自动保护当前数据"
                            onClicked: settingsPanel.restoreBackupRequested(settingsPanel.selectedBackupUrl())
                        }

                        ToolPill {
                            objectName: "refreshBackupsButton"
                            text: "刷新"
                            widthHint: 64
                            enabled: !settingsPanel.recoveryBusy
                            theme: settingsPanel.tokens
                            accessibleDescription: "重新扫描可恢复的备份"
                            onClicked: settingsPanel.refreshBackupsRequested()
                        }
                    }

                    Text {
                        objectName: "recoveryStatusText"
                        Layout.fillWidth: true
                        visible: settingsPanel.recoveryStatus.length > 0 ||
                                 settingsPanel.recoveryError.length > 0 ||
                                 settingsPanel.recoveryBusy
                        text: settingsPanel.recoveryError.length > 0
                              ? settingsPanel.recoveryError
                              : settingsPanel.recoveryBusy
                                ? "正在执行恢复中心操作…"
                                : settingsPanel.recoveryStatus
                        color: settingsPanel.recoveryError.length > 0
                               ? settingsPanel.tokens.danger
                               : settingsPanel.tokens.textSecondary
                        font.family: settingsPanel.tokens.fontUi
                        font.pixelSize: settingsPanel.tokens.sizeBody
                        wrapMode: Text.WordWrap
                        renderType: Text.NativeRendering
                        Accessible.role: settingsPanel.recoveryError.length > 0
                                         ? Accessible.AlertMessage : Accessible.StaticText
                        Accessible.name: text
                    }
                }
            }

            SettingsCard {
                objectName: "settingsDataCard"

                SectionHeading {
                    id: maintenanceHeading
                    title: "便签维护"
                    expanded: settingsPanel.maintenanceExpanded
                    onToggleRequested: settingsPanel.maintenanceExpanded = !settingsPanel.maintenanceExpanded
                }

                ColumnLayout {
                    visible: settingsPanel.maintenanceExpanded
                    Layout.fillWidth: true
                    spacing: settingsPanel.tokens.space2

                    Text {
                        Layout.fillWidth: true
                        text: "以下旧格式操作只作用于便签，不影响项目树；清理操作不可替代工作区备份。"
                        color: settingsPanel.tokens.textSecondary
                        font.pixelSize: settingsPanel.tokens.sizeBody
                        font.family: settingsPanel.tokens.fontUi
                        wrapMode: Text.WordWrap
                        renderType: Text.NativeRendering
                    }

                    GridLayout {
                        columns: settingsPanel.width < 420 ? 1 : 2
                        columnSpacing: settingsPanel.tokens.space2
                        rowSpacing: settingsPanel.tokens.space2
                        Layout.fillWidth: true

                        ToolPill {
                            objectName: "exportStickiesJsonButton"
                            text: "导出便签 JSON"
                            Layout.fillWidth: true
                            theme: settingsPanel.tokens
                            onClicked: settingsPanel.exportJsonRequested()
                        }

                        ToolPill {
                            objectName: "importStickiesJsonButton"
                            text: "导入便签 JSON"
                            Layout.fillWidth: true
                            theme: settingsPanel.tokens
                            onClicked: settingsPanel.importJsonRequested()
                        }

                        ToolPill {
                            objectName: "exportStickiesMarkdownButton"
                            text: "导出便签 Markdown"
                            Layout.fillWidth: true
                            theme: settingsPanel.tokens
                            onClicked: settingsPanel.exportMarkdownRequested()
                        }

                        ToolPill {
                            objectName: "clearCompletedStickiesButton"
                            text: "清理当前已完成"
                            danger: true
                            Layout.fillWidth: true
                            theme: settingsPanel.tokens
                            onClicked: settingsPanel.clearCompletedRequested()
                        }

                        ToolPill {
                            objectName: "clearAllCompletedStickiesButton"
                            text: "清理全部已完成"
                            danger: true
                            Layout.fillWidth: true
                            theme: settingsPanel.tokens
                            onClicked: settingsPanel.clearCompletedAllRequested()
                        }

                        ToolPill {
                            objectName: "clearCurrentStickiesButton"
                            text: "清空当前便签阶段"
                            danger: true
                            Layout.fillWidth: true
                            theme: settingsPanel.tokens
                            onClicked: settingsPanel.clearCurrentRequested()
                        }

                        ToolPill {
                            objectName: "clearAllStickiesButton"
                            text: settingsPanel.clearAllArmed ? "再次点击确认清空" : "清空全部便签"
                            danger: true
                            Layout.fillWidth: true
                            theme: settingsPanel.tokens
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

            ToolPill {
                objectName: "settingsSaveButton"
                text: "保存设置"
                widthHint: 120
                primary: true
                theme: settingsPanel.tokens
                Layout.alignment: Qt.AlignRight
                accessibleDescription: "保存 AI、界面和安全设置"
                onClicked: settingsPanel.saveRequested(
                               apiUrlField.text,
                               apiKeyDraftField.text,
                               modelField.text,
                               settingsPanel.allowLocalHttp,
                               settingsPanel.reduceMotion,
                               fontFamilyCombo.currentText,
                               fontSizeBox.value)
            }
        }
    }
}
