pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

Item {
    id: tabs

    readonly property ChronoTokens fallbackTokens: ChronoTokens {
        fontUi: tabs.uiFontFamily
        baseFontSize: tabs.uiFontSize
    }
    readonly property var tokens: tabs.theme ? tabs.theme : tabs.fallbackTokens

    property int stage: 0
    property bool allCompleted: false
    property var theme: null
    property string uiFontFamily: theme ? theme.fontUi : "Microsoft YaHei UI"
    property int uiFontSize: theme ? theme.baseFontSize : 12
    readonly property real contentHeight: 34

    signal stageRequested(int stage)
    signal toggleAllRequested()

    height: contentHeight

    function stageName(index) {
        return ["每天", "每周", "每月", "每年"][index]
    }

    component StageButton: Button {
        id: stageButton

        required property int stageIndex
        readonly property bool current: tabs.stage === stageIndex

        objectName: "stageTab" + stageIndex
        text: tabs.stageName(stageIndex)
        implicitWidth: Math.max(46, stageLabel.implicitWidth + tabs.tokens.space5)
        implicitHeight: tabs.contentHeight
        hoverEnabled: true
        activeFocusOnTab: true

        Accessible.role: Accessible.Button
        Accessible.name: text
        Accessible.description: current
                                ? "当前时间阶段：" + text
                                : "切换到" + text + "便签"

        contentItem: Text {
            id: stageLabel
            text: stageButton.text
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: stageButton.current ? tabs.tokens.accent : tabs.tokens.textSecondary
            font.family: tabs.uiFontFamily
            font.pixelSize: tabs.tokens.sizeLabel
            font.weight: stageButton.current ? Font.DemiBold : Font.Medium
            renderType: Text.NativeRendering
        }

        background: Rectangle {
            radius: tabs.tokens.radiusXs
            color: stageButton.down ? tabs.tokens.surfacePressed
                 : stageButton.hovered ? tabs.tokens.surfaceHover
                                       : tabs.tokens.transparent
            border.width: stageButton.visualFocus ? 2 : 0
            border.color: tabs.tokens.focusRing

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                width: Math.max(18, parent.width - tabs.tokens.space5)
                height: 2
                radius: 1
                color: tabs.tokens.accent
                visible: stageButton.current
            }
        }

        onClicked: tabs.stageRequested(stageIndex)
        Keys.onLeftPressed: {
            const target = (stageIndex + 3) % 4
            tabs.stageRequested(target)
            stageRepeater.itemAt(target).forceActiveFocus()
        }
        Keys.onRightPressed: {
            const target = (stageIndex + 1) % 4
            tabs.stageRequested(target)
            stageRepeater.itemAt(target).forceActiveFocus()
        }
    }

    Row {
        id: stageRow
        anchors.left: parent.left
        anchors.top: parent.top
        height: tabs.contentHeight
        spacing: tabs.tokens.space1

        Repeater {
            id: stageRepeater
            model: 4
            delegate: StageButton {
                required property int index
                stageIndex: index
            }
        }
    }

    Button {
        id: bulkButton
        objectName: "stageBulkMenuButton"
        anchors.right: parent.right
        anchors.top: parent.top
        implicitWidth: 72
        implicitHeight: tabs.contentHeight
        hoverEnabled: true
        activeFocusOnTab: true

        Accessible.role: Accessible.Button
        Accessible.name: "批量操作"
        Accessible.description: "打开当前时间阶段的批量操作菜单"

        contentItem: Row {
            spacing: tabs.tokens.space1

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "批量"
                color: tabs.tokens.textSecondary
                font.family: tabs.uiFontFamily
                font.pixelSize: tabs.tokens.sizeLabel
                font.weight: Font.Medium
                renderType: Text.NativeRendering
            }

            Image {
                anchors.verticalCenter: parent.verticalCenter
                width: 14
                height: 14
                source: "qrc:/assets/icons/chevron-down.svg"
                fillMode: Image.PreserveAspectFit
                smooth: true
            }
        }

        background: Rectangle {
            radius: tabs.tokens.radiusXs
            color: bulkButton.down ? tabs.tokens.surfacePressed
                 : bulkButton.hovered ? tabs.tokens.surfaceHover
                                      : tabs.tokens.transparent
            border.width: bulkButton.visualFocus ? 2 : 0
            border.color: tabs.tokens.focusRing
        }

        onClicked: bulkMenu.open()
    }

    Menu {
        id: bulkMenu
        objectName: "stageBulkMenu"
        x: tabs.width - width
        y: bulkButton.y + bulkButton.height + tabs.tokens.space1
        width: 196
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: tabs.tokens.surface
            radius: tabs.tokens.radiusSm
            border.width: 1
            border.color: tabs.tokens.border
        }

        MenuItem {
            id: toggleAllItem
            objectName: "stageToggleAllMenuItem"
            text: tabs.allCompleted ? "取消当前阶段全部完成" : "完成当前阶段全部便签"
            Accessible.description: tabs.allCompleted
                                    ? "将当前阶段的便签恢复为未完成"
                                    : "将当前阶段的便签全部标记为完成"

            contentItem: Text {
                text: toggleAllItem.text
                color: tabs.tokens.ink
                font.family: tabs.uiFontFamily
                font.pixelSize: tabs.tokens.sizeLabel
                verticalAlignment: Text.AlignVCenter
                leftPadding: tabs.tokens.space3
                rightPadding: tabs.tokens.space3
                renderType: Text.NativeRendering
            }

            background: Rectangle {
                color: toggleAllItem.highlighted ? tabs.tokens.surfaceHover
                                                 : tabs.tokens.transparent
                radius: tabs.tokens.radiusXs
            }

            onTriggered: tabs.toggleAllRequested()
        }
    }
}
