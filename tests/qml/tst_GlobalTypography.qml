import QtQuick
import QtTest
import "../../qml"

TestCase {
    name: "GlobalTypography"
    when: windowShown

    Item {
        id: harness
        width: 900
        height: 620

        ChronoTokens {
            id: appTokens
            fontUi: "Segoe UI"
            baseFontSize: 15
        }

        AppTitleBar {
            id: titlebar
            width: 900
            height: 40
            theme: appTokens
        }

        EventComposer {
            id: composer
            anchors.top: titlebar.bottom
            width: 420
            height: 42
            theme: appTokens
        }

        SearchBar {
            id: search
            anchors.top: composer.bottom
            width: 420
            open: true
            theme: appTokens
        }

        OverlayPanel {
            id: drawer
            anchors.top: search.bottom
            width: 320
            height: 420
            panel: "settings"
            theme: appTokens
            uiFontFamilies: ["Segoe UI", "Microsoft YaHei UI"]
        }
    }

    function test_sharedThemeFeedsMajorVisibleControls() {
        const aiButton = findChild(harness, "aiSummaryButton")
        const composerInput = findChild(harness, "composerInput")
        const searchInput = findChild(harness, "searchInput")
        const settingsTitle = findChild(harness, "settingsTitle")
        const fontCombo = findChild(harness, "settingsFontFamilyCombo")

        verify(aiButton !== null)
        verify(composerInput !== null)
        verify(searchInput !== null)
        verify(settingsTitle !== null)
        verify(fontCombo !== null)

        compare(aiButton.contentItem.font.family, "Segoe UI")
        compare(aiButton.contentItem.font.pixelSize, 15)
        compare(composerInput.font.family, "Segoe UI")
        compare(composerInput.font.pixelSize, 17)
        compare(searchInput.font.family, "Segoe UI")
        compare(searchInput.font.pixelSize, 16)
        compare(settingsTitle.font.family, "Segoe UI")
        compare(fontCombo.font.family, "Segoe UI")
    }
}
