from Qt import QtGui, QtCore, QtWidgets
from maya import cmds
from utils import toggleBlockSignals
import math


class ButtonWithValue(QtWidgets.QPushButton):
    def __init__(
        self,
        parent=None,
        usePow=True,
        name="prune",
        minimumValue=-1,
        defaultValue=2,
        maximumValue=10,
        step=1,
        clickable=True,
        minHeight=24,
    ):
        self.usePow = usePow
        self.name = name
        self.minimumValue = minimumValue
        self.maximumValue = maximumValue
        self.defaultValue = defaultValue
        self.step = step
        self.clickable = clickable
        self.optionVarName = "ButtonWithValue_" + name
        super(ButtonWithValue, self).__init__(parent)
        self.setMinimumHeight(minHeight)
        self._metrics = QtGui.QFontMetrics(self.font())
        self.getValuePrecision()

    def mousePressEvent(self, event):
        if self.clickable:
            super(ButtonWithValue, self).mousePressEvent(event)

    def getValuePrecision(self):
        self.precision = (
            cmds.optionVar(q=self.optionVarName)
            if cmds.optionVar(exists=self.optionVarName)
            else self.defaultValue
        )
        self.updateName()

    def wheelEvent(self, event):
        val = event.angleDelta().y()
        if val > 0.0:
            self.precision += self.step
        else:
            self.precision -= self.step
        if self.precision < self.minimumValue:
            self.precision = self.minimumValue
        if self.precision > self.maximumValue:
            self.precision = self.maximumValue

        if self.step != 1:
            self.precision = round(self.precision, 1)
        self.updateName()

    def updateName(self):
        if self.usePow:
            self.precisionValue = math.pow(10, self.precision * -1)
            theText = " {0} {1} ".format(self.name, self.precisionValue)
        else:
            theText = " {0} {1} ".format(self.name, self.precision)
        self.setText(theText)
        self.setMinimumWidth(self._metrics.width(theText) + 6)

        if self.step == 1:
            cmds.optionVar(intValue=[self.optionVarName, self.precision])
        else:
            cmds.optionVar(floatValue=[self.optionVarName, self.precision])


###################################################################################
#
#   the slider setting
#
###################################################################################
class ValueSetting(QtWidgets.QWidget):
    theStyleSheet = """QDoubleSpinBox {color: black; background-color:rgb(200,200,200) ; border: 1px solid black;text-align: center;}
                       QDoubleSpinBox:disabled {color: grey; background-color:rgb(170,170,170) ; border: 1px solid black;text-align: center;}
                    """

    def __init__(self, parent=None, singleStep=0.1, precision=1):
        super(ValueSetting, self).__init__(parent=None)
        self.theProgress = ProgressItem("skinVal", szrad=0, value=50)
        self.setAddMode(True)

        self.theProgress.prt = self
        self.mainWindow = parent
        # self.displayText = QtWidgets.QLabel (self)

        layout = QtWidgets.QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        self.theSpinner = QtWidgets.QDoubleSpinBox(self)
        self.theSpinner.setRange(-16777214, 16777215)
        self.theSpinner.setSingleStep(singleStep)
        self.theSpinner.setDecimals(precision)
        self.theSpinner.setButtonSymbols(QtWidgets.QAbstractSpinBox.NoButtons)
        self.theSpinner.setStyleSheet(self.theStyleSheet)

        # self.theSpinner.valueChanged.connect (self.valueEntered)

        newPolicy = QtWidgets.QSizePolicy(
            QtWidgets.QSizePolicy.Minimum, QtWidgets.QSizePolicy.Minimum
        )
        self.theSpinner.setMaximumWidth(40)
        newPolicy.setHorizontalStretch(0)
        newPolicy.setVerticalStretch(0)
        self.theSpinner.setSizePolicy(newPolicy)

        self.theProgress.setMaximumHeight(18)

        self.theLineEdit = None
        for chd in self.theSpinner.children():
            if isinstance(chd, QtWidgets.QLineEdit):
                self.theLineEdit = chd
                break
        self.theLineEdit.returnPressed.connect(self.spinnerValueEntered)

        self.theSpinner.focusInEvent = self.theSpinner_focusInEvent

        layout.addWidget(self.theSpinner)
        layout.addWidget(self.theProgress)

        self.theProgress.valueChanged.connect(self.setVal)

    def theSpinner_focusInEvent(self, event):
        QtWidgets.QDoubleSpinBox.focusInEvent(self.theSpinner, event)
        cmds.evalDeferred(self.theLineEdit.selectAll)

    def preSet(self):
        return True

    def doSet(self, theVal):
        print theVal

    def postSet(self):
        return

    def spinnerValueEntered(self):
        theVal = self.theSpinner.value()
        # print "value Set {0}".format (theVal)

        self.preSet()
        self.doSet(theVal / 100.0)
        if self.theProgress.autoReset:
            self.setVal(self.theProgress.releasedValue)
        else:
            self.theProgress.applyVal(theVal / 100.0)
        self.postSet()

    def setVal(self, val):
        # theVal = val/100.
        if self.addMode:
            theVal = (val - 50) / 50.0
        else:
            theVal = val / 100.0
        # ------- SETTING FUNCTION ---------------------
        if self.theProgress.startDrag:
            self.doSet(theVal)
        else:
            self.postSet()

        # else : # wheelEvent
        self.theSpinner.setValue(theVal * 100.0)

    def setAddMode(self, addMode, autoReset=True):
        if addMode:
            self.addMode = True
            self.theProgress.autoReset = autoReset
            self.theProgress.releasedValue = 50.0
        else:
            self.addMode = False
            self.theProgress.autoReset = autoReset
            self.theProgress.releasedValue = 0.0
        with toggleBlockSignals([self.theProgress]):
            self.theProgress.setValue(self.theProgress.releasedValue)


# for the weightEditor
class ValueSettingWE(ValueSetting):
    def preSet(self):
        return self.mainWindow.prepareToSetValue()

    def doSet(self, theVal):
        return self.mainWindow.doAddValue(theVal)

    def postSet(self):
        return self.mainWindow.dataOfSkin.postSkinSet()


class ProgressItem(QtWidgets.QProgressBar):
    theStyleSheet = """QProgressBar {{color: black; background-color:{bgColor} ; border: 1px solid black;text-align: center;
    border-bottom-right-radius: {szrad}px;
    border-bottom-left-radius: {szrad}px;
    border-top-right-radius: {szrad}px;
    border-top-left-radius: {szrad}px;}}
    QProgressBar:disabled {{color: black; background-color:{bgColorDisabled} ; border: 1px solid black;text-align: center;
    border-bottom-right-radius: {szrad}px;
    border-bottom-left-radius: {szrad}px;
    border-top-right-radius: {szrad}px;
    border-top-left-radius: {szrad}px;}}            
    QProgressBar::chunk {{background:{chunkColor};
    border-bottom-right-radius: {szrad}px;
    border-bottom-left-radius: {szrad}px;
    border-top-right-radius: {szrad}px;
    border-top-left-radius: {szrad}px;}}
    QProgressBar::chunk:disabled {{background:{chunkColorDisabled};
    border-bottom-right-radius: {szrad}px;
    border-bottom-left-radius: {szrad}px;
    border-top-right-radius: {szrad}px;
    border-top-left-radius: {szrad}px;}}
    """
    prt = None

    def __init__(self, theName, value=0, **kwargs):
        super(ProgressItem, self).__init__()
        self.multiplier = 1

        # self.setFormat (theName+" %p%")
        self.setFormat("")
        self.dicStyleSheet = dict(
            {
                "szrad": 7,
                "bgColor": "rgb(136,136,136)",
                "bgColorDisabled": "rgb(136,136,136)",
                "chunkColor": "rgb(200,200,200)",
                "chunkColorDisabled": "rgb(170,170,170)",
            },
            **kwargs
        )

        self.setStyleSheet(self.theStyleSheet.format(**self.dicStyleSheet))
        self.setValue(value)

    def changeColor(self, **kwargs):
        self.dicStyleSheet = dict(
            {"szrad": 7, "bgColor": "rgb(200,200,230)", "chunkColor": "#FF0350"}, **kwargs
        )
        self.setStyleSheet(self.theStyleSheet.format(**self.dicStyleSheet))

    def setEnabled(self, val):
        super(ProgressItem, self).setEnabled(val)
        print "set Enalbeld {0}".format(val)
        if not val:
            tmpDic = dict(
                self.dicStyleSheet,
                **{"szrad": 7, "bgColor": "rgb(100,100,100)", "chunkColor": "#FF0350"}
            )
            self.setStyleSheet(self.theStyleSheet.format(**tmpDic))
        else:
            self.setStyleSheet(self.theStyleSheet.format(**self.dicStyleSheet))

    def applyVal(self, val):
        # print "applyVal {0}".format (val)
        val *= self.multiplier
        if self.minimum() == -100:
            val = val * 2 - 1
        self.setValue(int(val * 100))

    """
    def wheelEvent  (self, e):
        delta = e.delta ()
        #print delta
        val = self.value () /100.
        if self.minimum () == -100 : val = val*.5 + .5

        offset = -.1 if delta < 0 else .1
        val += offset
        if val >1. : val = 1.0
        elif val <0. : val = 0.
        self.applyVal (val)
    """

    startDrag = False

    def mousePressEvent(self, event):
        if event.modifiers() == QtCore.Qt.ControlModifier or event.button() != QtCore.Qt.LeftButton:
            super(ProgressItem, self).mousePressEvent(event)
            self.startDrag = False
        else:
            cmds.undoInfo(stateWithoutFlush=False)
            # ------------- PREPARE FUNCTION -------------------------------------------------------------------------------------
            self.startDrag = (
                self.prt.preSet()
            )  # self.mainWindow.prepareToSetValue()#self.prt.preSet()
            if self.startDrag:
                self.applyTheEvent(event)

    def mouseReleaseEvent(self, event):
        self.startDrag = False
        if event.modifiers() == QtCore.Qt.ControlModifier or event.button() != QtCore.Qt.LeftButton:
            super(ProgressItem, self).mouseReleaseEvent(event)
        else:
            # print "releasing"
            self.setMouseTracking(False)
            cmds.undoInfo(stateWithoutFlush=True)
            super(ProgressItem, self).mouseReleaseEvent(event)
        if self.autoReset:
            self.setValue(self.releasedValue)

    def applyTheEvent(self, e):
        shitIsHold = e.modifiers() == QtCore.Qt.ShiftModifier
        theWdth = self.width()
        # print e.mouseButtons()
        # print "moving {0}".format (e.x())
        val = e.x() / float(theWdth)
        if shitIsHold:
            val = round(val * 4.0) / 4.0
        # print (val)
        if val > 1.0:
            val = 1.0
        elif val < 0.0:
            val = 0.0
        self.applyVal(val)

    def mouseMoveEvent(self, event):
        isLeft = event.button() == QtCore.Qt.LeftButton
        isCtr = event.modifiers() == QtCore.Qt.ControlModifier
        # print "mouseMoveEvent ", isLeft, isCtr
        if self.startDrag:
            self.applyTheEvent(event)
        super(ProgressItem, self).mouseMoveEvent(event)
