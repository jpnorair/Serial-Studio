/*
 * Copyright (c) 2020-2021 Alex Spataru <https://github.com/alex-spataru>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

import QtQuick 2.12
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12

import SerialStudio 1.0

import "."

Window {
    id: root

    //
    // Window properties
    //
    spacing: -1
    gradient: true
    implicitWidth: 260
    visible: opacity > 0
    opacity: enabled ? 1 : 0
    backgroundColor: "#09090c"
    icon.source: "qrc:/icons/chart.svg"
    implicitHeight: implicitWidth + 96

    //
    // Colors
    //
    property color tankColor: "#e6e0b2"
    property color valueColor: "#517497"
    property color titleColor: "#8ecd9d"
    property color emptyColor: {
        if ((root.measMinValue < root.minimumValue) || (root.measMaxValue > root.maximumValue))
            return "#8a2728";
        else
            return "#00811d";
    }
    property color testColor:  {
        if ((root.measMinValue < root.minimumValue) || (root.measMaxValue > root.maximumValue))
            return "#ff303b";
        else
            return "#00ff28";
    }

    //
    // Custom properties
    //
    property real minlevel: 0
    property real maxlevel: 0
    property string units: ""
    property int datasetIndex: 0
    property real tankRadius: 20
    property real currentValue: 0
    property real minimumValue: 0
    property real maximumValue: 10
    property real measMinValue: Infinity
    property real measMaxValue: -Infinity

    //
    // Connections with widget manager
    //
    Connections {
        target: Cpp_UI_WidgetProvider

        //*! Optimize this function
        function onDataChanged() {
            root.updateValues()
        }
    }

    //
    // Updates the internal values of the bar widget
    //
    //*! Optimize this function
    //   About 6% of the UI thread is spent here
    function updateValues() {
        if (Cpp_UI_WidgetProvider.barDatasetCount() > root.datasetIndex) {
            var level = 0

            root.minimumValue = Cpp_UI_WidgetProvider.barMin(root.datasetIndex)
            root.maximumValue = Cpp_UI_WidgetProvider.barMax(root.datasetIndex)
            root.currentValue = Cpp_UI_WidgetProvider.bar(root.datasetIndex)
            root.title = Cpp_UI_WidgetProvider.barDatasetAt(root.datasetIndex).title
            root.units = Cpp_UI_WidgetProvider.barDatasetAt(root.datasetIndex).units

            if (root.maximumValue > root.minimumValue)  {
                var range = root.maximumValue - root.minimumValue
                var relat = root.currentValue - root.minimumValue
                level = relat / range
            }

            if (level > 1) {
                level = 1
            }

            if (root.currentValue < root.measMinValue) {
                root.measMinValue = root.currentValue
                root.minlevel = level
            }
            if (root.currentValue > root.measMaxValue) {
                root.measMaxValue = root.currentValue
                root.maxlevel = level
            }

        }

        else {
            root.title = ""
            root.units = ""
            root.currentValue = 0
            root.minimumValue = 0
            root.maximumValue = 0
        }
    }

    //
    // Layout
    //
    RowLayout {
        spacing: app.spacing * 4

        anchors {
            fill: parent
            margins: app.spacing * 4
            leftMargin: app.spacing * 8
            rightMargin: app.spacing * 8
        }

        //
        // Bar rectangle
        //
        Rectangle {
            id: tank
            border.width: 2
            color: root.emptyColor
            Layout.fillHeight: true
            border.color: root.tankColor
            Layout.minimumWidth: root.width * 0.17
            Layout.maximumWidth: root.width * 0.17

            //
            // Level indicator
            //
            Rectangle {
                anchors.fill: parent
                anchors.topMargin: (1 - root.maxlevel) * tank.height
                anchors.bottomMargin: (1 - root.minlevel) * tank.height
                border.width: 1
                color: root.testColor
                border.color: root.testColor
            }
        }

        //
        // Level text
        //
        ColumnLayout {
            spacing: app.spacing
            Layout.fillHeight: true

            Label {
                color: root.titleColor
                font.family: app.monoFont
                Layout.alignment: Qt.AlignHCenter
                text: root.maximumValue.toFixed(2) + " " + root.units
            }

            Rectangle {
                width: 2
                color: root.valueColor
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignHCenter
            }

            Label {
                font.bold: true
                font.pixelSize: 16
                color: root.titleColor
                font.family: app.monoFont
                Layout.alignment: Qt.AlignHCenter
                text: (root.currentValue > root.maximumValue ? root.maximumValue.toFixed(2) :
                                                               root.currentValue.toFixed(2)) + " " + root.units

                Rectangle {
                    border.width: 1
                    anchors.fill: parent
                    color: "transparent"
                    border.color: root.titleColor
                    anchors.margins: -app.spacing
                }
            }

            Rectangle {
                width: 2
                color: root.valueColor
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignHCenter
            }

            Label {
                color: root.titleColor
                font.family: app.monoFont
                Layout.alignment: Qt.AlignHCenter
                text: root.minimumValue.toFixed(2) + " " + root.units
            }
        }
    }
}
