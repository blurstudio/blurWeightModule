#include "skinBrushTool.h"

#include "skinBrushFlags.h"

// Macro for the press/drag/release methods in case there is nothing
// selected or the tool gets applied outside any geometry. If the actual
// MStatus would get returned an error can get listed in terminal on
// Linux. But it's unnecessary and needs to be avoided. Therefore a
// kSuccess is returned just for the sake of being invisible.
#define CHECK_MSTATUS_AND_RETURN_SILENT(status) \
    if (status != MStatus::kSuccess) return MStatus::kSuccess;

// ---------------------------------------------------------------------
// the tool
// ---------------------------------------------------------------------

// ---------------------------------------------------------------------
// general methods for the tool command
// ---------------------------------------------------------------------

skinBrushTool::skinBrushTool() {
    setCommandString("brSkinBrushCmd");

    colorVal = MColor(1.0, 0.0, 0.0);
    curveVal = 2;
    drawBrushVal = true;
    drawRangeVal = true;
    moduleImportString = MString("from brSkinBrush_pythonFunctions import ");
    enterToolCommandVal = "";
    exitToolCommandVal = "";
    fractionOversamplingVal = false;
    ignoreLockVal = false;
    lineWidthVal = 1;
    messageVal = 2;
    oversamplingVal = 1;
    rangeVal = 0.5;
    sizeVal = 5.0;
    strengthVal = 0.25;
    smoothStrengthVal = 1.0;
    undersamplingVal = 2;
    volumeVal = false;
    coverageVal = true;

    pruneWeights = 0.0001;
    commandIndex = 0;      // add
    soloColorTypeVal = 1;  // 1 lava
    soloColorVal = 0;
    postSetting = true;
}

skinBrushTool::~skinBrushTool() {}

void* skinBrushTool::creator() { return new skinBrushTool; }

bool skinBrushTool::isUndoable() const { return true; }

MSyntax skinBrushTool::newSyntax() {
    MSyntax syntax;

    syntax.addFlag(kColorRFlag, kColorRFlagLong, MSyntax::kDouble);
    syntax.addFlag(kColorGFlag, kColorGFlagLong, MSyntax::kDouble);
    syntax.addFlag(kColorBFlag, kColorBFlagLong, MSyntax::kDouble);
    syntax.addFlag(kCurveFlag, kCurveFlagLong, MSyntax::kLong);
    syntax.addFlag(kDrawBrushFlag, kDrawBrushFlagLong, MSyntax::kBoolean);
    syntax.addFlag(kDrawRangeFlag, kDrawRangeFlagLong, MSyntax::kBoolean);
    syntax.addFlag(kImportPythonFlag, kImportPythonFlagLong, MSyntax::kString);
    syntax.addFlag(kEnterToolCommandFlag, kEnterToolCommandFlagLong, MSyntax::kString);
    syntax.addFlag(kExitToolCommandFlag, kExitToolCommandFlagLong, MSyntax::kString);
    syntax.addFlag(kFractionOversamplingFlag, kFractionOversamplingFlagLong, MSyntax::kBoolean);
    syntax.addFlag(kIgnoreLockFlag, kIgnoreLockFlagLong, MSyntax::kBoolean);
    syntax.addFlag(kLineWidthFlag, kLineWidthFlagLong, MSyntax::kLong);
    syntax.addFlag(kMessageFlag, kMessageFlagLong, MSyntax::kLong);
    syntax.addFlag(kOversamplingFlag, kOversamplingFlagLong, MSyntax::kLong);
    syntax.addFlag(kRangeFlag, kRangeFlagLong, MSyntax::kDouble);
    syntax.addFlag(kSizeFlag, kSizeFlagLong, MSyntax::kDouble);
    syntax.addFlag(kStrengthFlag, kStrengthFlagLong, MSyntax::kDouble);
    syntax.addFlag(kUndersamplingFlag, kUndersamplingFlagLong, MSyntax::kLong);
    syntax.addFlag(kVolumeFlag, kVolumeFlagLong, MSyntax::kBoolean);

    syntax.addFlag(kPruneWeightsFlag, kPruneWeightsFlagLong, MSyntax::kDouble);
    syntax.addFlag(kSmoothStrengthFlag, kSmoothStrengthFlagLong, MSyntax::kDouble);
    syntax.addFlag(kCommandIndexFlag, kCommandIndexFlagLong, MSyntax::kLong);
    syntax.addFlag(kSoloColorFlag, kSoloColorFlagLong, MSyntax::kLong);
    syntax.addFlag(kSoloColorTypeFlag, kSoloColorTypeFlagLong, MSyntax::kLong);
    syntax.addFlag(kCoverageFlag, kCoverageLong, MSyntax::kBoolean);

    syntax.addFlag(kUseColorSetWhilePaintingFlag, kUseColorSetWhilePaintingFlagLong,
                   MSyntax::kBoolean);
    syntax.addFlag(kMeshDragDrawTrianglesFlag, kMeshDragDrawTrianglesFlagLong, MSyntax::kBoolean);
    syntax.addFlag(kMeshDragDrawEdgesFlag, kMeshDragDrawEdgesFlagLong, MSyntax::kBoolean);
    syntax.addFlag(kMeshDragDrawPointsFlag, kMeshDragDrawPointsFlagLong, MSyntax::kBoolean);
    syntax.addFlag(kMeshDragDrawTransFlag, kMeshDragDrawTransFlagLong, MSyntax::kBoolean);

    // syntax.addFlag(kPickMaxInfluenceFlag, kPickMaxInfluenceFlagLong, MSyntax::kBoolean);
    syntax.addFlag(kSmoothRepeatFlag, kSmoothRepeatFlagLong, MSyntax::kLong);

    syntax.addFlag(kInfluenceIndexFlag, kInfluenceIndexFlagLong, MSyntax::kLong);
    syntax.addFlag(kPostSettingFlag, kPostSettingFlagLong, MSyntax::kBoolean);

    return syntax;
}

MStatus skinBrushTool::parseArgs(const MArgList& args) {
    MStatus status = MStatus::kSuccess;

    MArgDatabase argData(syntax(), args);

    if (argData.isFlagSet(kColorRFlag)) {
        double value;
        status = argData.getFlagArgument(kColorRFlag, 0, value);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        colorVal = MColor((float)value, colorVal.g, colorVal.b);
    }
    if (argData.isFlagSet(kColorGFlag)) {
        double value;
        status = argData.getFlagArgument(kColorGFlag, 0, value);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        colorVal = MColor(colorVal.r, (float)value, colorVal.b);
    }
    if (argData.isFlagSet(kColorBFlag)) {
        double value;
        status = argData.getFlagArgument(kColorBFlag, 0, value);
        CHECK_MSTATUS_AND_RETURN_IT(status);
        colorVal = MColor(colorVal.r, colorVal.g, (float)value);
    }
    if (argData.isFlagSet(kCurveFlag)) {
        status = argData.getFlagArgument(kCurveFlag, 0, curveVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kDrawBrushFlag)) {
        status = argData.getFlagArgument(kDrawBrushFlag, 0, drawBrushVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kDrawRangeFlag)) {
        status = argData.getFlagArgument(kDrawRangeFlag, 0, drawRangeVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kImportPythonFlag)) {
        status = argData.getFlagArgument(kImportPythonFlag, 0, moduleImportString);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kEnterToolCommandFlag)) {
        status = argData.getFlagArgument(kEnterToolCommandFlag, 0, enterToolCommandVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kExitToolCommandFlag)) {
        status = argData.getFlagArgument(kExitToolCommandFlag, 0, exitToolCommandVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kFractionOversamplingFlag)) {
        status = argData.getFlagArgument(kFractionOversamplingFlag, 0, fractionOversamplingVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kIgnoreLockFlag)) {
        status = argData.getFlagArgument(kIgnoreLockFlag, 0, ignoreLockVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kLineWidthFlag)) {
        status = argData.getFlagArgument(kLineWidthFlag, 0, lineWidthVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kMessageFlag)) {
        status = argData.getFlagArgument(kMessageFlag, 0, messageVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kOversamplingFlag)) {
        status = argData.getFlagArgument(kOversamplingFlag, 0, oversamplingVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kRangeFlag)) {
        status = argData.getFlagArgument(kRangeFlag, 0, rangeVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kSizeFlag)) {
        status = argData.getFlagArgument(kSizeFlag, 0, sizeVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kStrengthFlag)) {
        status = argData.getFlagArgument(kStrengthFlag, 0, strengthVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kPruneWeightsFlag)) {
        status = argData.getFlagArgument(kPruneWeightsFlag, 0, pruneWeights);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kUndersamplingFlag)) {
        status = argData.getFlagArgument(kUndersamplingFlag, 0, undersamplingVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kVolumeFlag)) {
        status = argData.getFlagArgument(kVolumeFlag, 0, volumeVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kSmoothStrengthFlag)) {
        status = argData.getFlagArgument(kSmoothStrengthFlag, 0, smoothStrengthVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }

    if (argData.isFlagSet(kCoverageFlag)) {
        status = argData.getFlagArgument(kCoverageFlag, 0, coverageVal);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kInfluenceIndexFlag)) {
        status = argData.getFlagArgument(kInfluenceIndexFlag, 0, influenceIndex);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }

    if (argData.isFlagSet(kUseColorSetWhilePaintingFlag)) {
        status =
            argData.getFlagArgument(kUseColorSetWhilePaintingFlag, 0, useColorSetsWhilePainting);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kMeshDragDrawTrianglesFlag)) {
        status = argData.getFlagArgument(kMeshDragDrawTrianglesFlag, 0, drawTriangles);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kMeshDragDrawEdgesFlag)) {
        status = argData.getFlagArgument(kMeshDragDrawEdgesFlag, 0, drawEdges);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kMeshDragDrawPointsFlag)) {
        status = argData.getFlagArgument(kMeshDragDrawPointsFlag, 0, drawPoints);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    if (argData.isFlagSet(kMeshDragDrawTransFlag)) {
        status = argData.getFlagArgument(kMeshDragDrawTransFlag, 0, drawTransparency);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }

    return status;
}

// ---------------------------------------------------------------------
// main methods for the tool command
// ---------------------------------------------------------------------
MStatus skinBrushTool::doIt(const MArgList& args) {
    // MGlobal::displayInfo(MString("---------------- [skinBrushTool::doIt]------------------"));

    MStatus status = MStatus::kSuccess;

    status = parseArgs(args);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    return redoIt();
}

MStatus skinBrushTool::redoIt() {
    MStatus status = MStatus::kSuccess;

    MGlobal::displayInfo(MString("skinBrushTool::redoIt is CALLED !!!! commandIndex : ") +
                         this->commandIndex);

    // Apply the redo weights and get the current weights for undo.
    MFnSkinCluster skinFn(skinObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    if (this->commandIndex < 6 && this->redoWeights.length()) {
        MFnSingleIndexedComponent compFn;
        MObject weightsObj = compFn.create(MFn::kMeshVertComponent);
        compFn.addElements(this->undoVertices);

        skinFn.setWeights(meshDag, weightsObj, influenceIndices, redoWeights, true);  // normalize);
        // refresh the tool points positions ---------
    }
    if (this->commandIndex >= 6) {
        MGlobal::displayInfo("redo it updateSurface : lock / unlock vertices");

        MObjectArray objectsDeformed;
        skinFn.getOutputGeometry(objectsDeformed);
        MFnDependencyNode deformedNameMesh(objectsDeformed[0]);
        MPlug lockedVerticesPlug = deformedNameMesh.findPlug("lockedVertices", &stat);
        if (MS::kSuccess != status) {
            MGlobal::displayError(MString("cant find lockerdVertices plug"));
            return status;
        }
        // now set the value ---------------------------
        MFnIntArrayData tmpIntArray;

        MIntArray theArrayValues;
        for (unsigned int vtx = 0; vtx < redoLocks.length(); ++vtx) {
            if (redoLocks[vtx] == 1) theArrayValues.append(vtx);
        }
        status = lockedVerticesPlug.setValue(
            tmpIntArray.create(theArrayValues));  // to set the attribute

        // we need a hard refresh of invalidate for the undo / redo ---

        MFnMesh meshFn;
        meshFn.setObject(meshDag);
        meshFn.updateSurface();
    }
    callBrushRefresh();
    return MStatus::kSuccess;
}

MStatus skinBrushTool::undoIt() {
    MStatus status = MStatus::kSuccess;

    MGlobal::displayInfo(MString("skinBrushTool::undoIt is CALLED ! commandIndex : ") +
                         this->commandIndex);

    MFnSkinCluster skinFn(skinObj, &status);
    CHECK_MSTATUS_AND_RETURN_IT(status);

    if (this->commandIndex < 6 && this->undoWeights.length()) {
        MFnSingleIndexedComponent compFn;
        MObject weightsObj = compFn.create(MFn::kMeshVertComponent);
        compFn.addElements(this->undoVertices);

        // Apply the previous weights and get the current weights for
        // redo.
        // skinFn.setWeights(meshDag, weightsObj, influenceIndices, this->undoWeights, normalize,
        // &redoWeights);
        skinFn.setWeights(meshDag, weightsObj, influenceIndices, this->undoWeights, true,
                          &redoWeights);
    }

    if (this->commandIndex >= 6) {
        MGlobal::displayInfo("undo it with refresh: lock / unlock vertices");

        MObjectArray objectsDeformed;
        skinFn.getOutputGeometry(objectsDeformed);
        MFnDependencyNode deformedNameMesh(objectsDeformed[0]);
        MPlug lockedVerticesPlug = deformedNameMesh.findPlug("lockedVertices", &stat);
        if (MS::kSuccess != status) {
            MGlobal::displayError(MString("cant find lockerdVertices plug"));
            return status;
        }
        // now set the value ---------------------------
        MFnIntArrayData tmpIntArray;

        MIntArray theArrayValues;
        for (unsigned int vtx = 0; vtx < undoLocks.length(); ++vtx) {
            if (undoLocks[vtx] == 1) theArrayValues.append(vtx);
        }
        status = lockedVerticesPlug.setValue(
            tmpIntArray.create(theArrayValues));  // to set the attribute
        // we need a hard refresh of invalidate for the undo / redo ---
        MFnMesh meshFn;
        meshFn.setObject(meshDag);
        meshFn.updateSurface();
    }
    callBrushRefresh();
    /*
    MGlobal::getActiveSelectionList(redoSelection);
    MGlobal::getHiliteList(redoHilite);

    MGlobal::setActiveSelectionList(undoSelection);
    MGlobal::setHiliteList(undoHilite);
    */
    return MStatus::kSuccess;
}
MStatus skinBrushTool::callBrushRefresh() {
    /*
    ----------------
    ---------------- VERY IMPORTANT
    ---------------- refresh the tool points positions, normals, skin weight stored, and vertex
    colors  ---------
    ----------------
    ----------------
    */

    MString cmd;
    // cmd = "brSkinBrushContext -edit -refresh 1 brSkinBrushContext1;";
    // cmd = "evalDeferred  ( \"brSkinBrushContext -edit -refresh 1 brSkinBrushContext1\");";
    // cmd = "brSkinBrushContext -edit -refreshPointsNormals 1 brSkinBrushContext1;";
    // cmd = "evalDeferred  ( \"brSkinBrushContext -edit -refreshPointsNormals 1
    // brSkinBrushContext1\");"; MGlobal::executeCommandOnIdle(cmd);
    MString lst = "";
    int i, nbElements;
    nbElements = this->undoVertices.length();
    if (nbElements > 0) {
        for (i = 0; i < (nbElements - 1); ++i) {
            lst += this->undoVertices[i];
            lst += MString(", ");
        }
        lst += this->undoVertices[nbElements - 1];
        cmd = MString("cmds.brSkinBrushContext('brSkinBrushContext1', e=True,");
        cmd += MString("listVerticesIndices = [") + lst + MString("])");

        /*
        MString melCmd = MString ("evalDeferred(\"python (\\\"")+ cmd +MString("\\\")\");" ) ;
        MGlobal::displayInfo(melCmd);
        MGlobal::executeCommandOnIdle(melCmd);
        */
        MGlobal::executePythonCommandOnIdle(cmd);
    }

    return MStatus::kSuccess;
}

MStatus skinBrushTool::finalize() {
    // Store the current settings as an option var. This way they are
    // properly available for the next usage.
    // MGlobal::displayInfo("skinBrushTool::finalize\n");
    MString cmd;
    cmd = "brSkinBrushContext -edit ";
    cmd += "-image1 \"brSkinBrush.svg\" -image2 \"vacantCell.png\" -image3 \"vacantCell.png\"";
    cmd += " " + MString(kColorRFlag) + " ";
    cmd += colorVal.r;
    cmd += " " + MString(kColorGFlag) + " ";
    cmd += colorVal.g;
    cmd += " " + MString(kColorBFlag) + " ";
    cmd += colorVal.b;
    cmd += " " + MString(kCurveFlag) + " ";
    cmd += curveVal;

    cmd += " " + MString(kCommandIndexFlag) + " ";
    cmd += commandIndex;
    cmd += " " + MString(kSoloColorFlag) + " ";
    cmd += soloColorVal;
    cmd += " " + MString(kSoloColorTypeFlag) + " ";
    cmd += soloColorTypeVal;
    cmd += " " + MString(kCoverageFlag) + " ";
    cmd += coverageVal;
    cmd += " " + MString(kMessageFlag) + " ";
    cmd += messageVal;

    cmd += " " + MString(kDrawBrushFlag) + " ";
    cmd += drawBrushVal;
    cmd += " " + MString(kDrawRangeFlag) + " ";
    cmd += drawRangeVal;
    cmd += " " + MString(kImportPythonFlag) + " ";
    cmd += "\"" + moduleImportString + "\"";
    cmd += " " + MString(kEnterToolCommandFlag) + " ";
    cmd += "\"" + enterToolCommandVal + "\"";
    cmd += " " + MString(kExitToolCommandFlag) + " ";
    cmd += "\"" + exitToolCommandVal + "\"";
    cmd += " " + MString(kFractionOversamplingFlag) + " ";
    cmd += fractionOversamplingVal;
    cmd += " " + MString(kIgnoreLockFlag) + " ";
    cmd += ignoreLockVal;
    cmd += " " + MString(kLineWidthFlag) + " ";
    cmd += lineWidthVal;
    cmd += " " + MString(kOversamplingFlag) + " ";
    cmd += oversamplingVal;
    cmd += " " + MString(kRangeFlag) + " ";
    cmd += rangeVal;
    cmd += " " + MString(kSizeFlag) + " ";
    cmd += sizeVal;
    cmd += " " + MString(kStrengthFlag) + " ";
    cmd += strengthVal;
    cmd += " " + MString(kSmoothStrengthFlag) + " ";
    cmd += smoothStrengthVal;
    cmd += " " + MString(kPruneWeightsFlag) + " ";
    cmd += pruneWeights;
    cmd += " " + MString(kUndersamplingFlag) + " ";
    cmd += undersamplingVal;
    cmd += " " + MString(kVolumeFlag) + " ";
    cmd += volumeVal;
    cmd += " " + MString(kPostSettingFlag) + " ";
    cmd += postSetting;
    cmd += " " + MString(kInfluenceNameFlag) + " ";
    cmd += influenceName;

    cmd += " " + MString(kSmoothRepeatFlag) + " ";
    cmd += smoothRepeat;

    cmd += " " + MString(kUseColorSetWhilePaintingFlag) + " ";
    cmd += useColorSetsWhilePainting;
    cmd += " " + MString(kMeshDragDrawTrianglesFlag) + " ";
    cmd += drawTriangles;
    cmd += " " + MString(kMeshDragDrawEdgesFlag) + " ";
    cmd += drawEdges;
    cmd += " " + MString(kMeshDragDrawPointsFlag) + " ";
    cmd += drawPoints;
    cmd += " " + MString(kMeshDragDrawTransFlag) + " ";
    cmd += drawTransparency;

    cmd += " brSkinBrushContext1;";

    MGlobal::setOptionVarValue("brSkinBrushContext1", cmd);

    // Finalize the command by adding it to the undo queue and the
    // journal.
    MArgList command;
    command.addArg(commandString());

    return MPxToolCommand::doFinalize(command);
}

// ---------------------------------------------------------------------
// getting values from the command flags
// ---------------------------------------------------------------------

void skinBrushTool::setColor(MColor value) { colorVal = value; }

void skinBrushTool::setCurve(int value) { curveVal = value; }

void skinBrushTool::setDrawBrush(bool value) { drawBrushVal = value; }

void skinBrushTool::setDrawRange(bool value) { drawRangeVal = value; }

void skinBrushTool::setPythonImportPath(MString value) { moduleImportString = value; }

void skinBrushTool::setEnterToolCommand(MString value) { enterToolCommandVal = value; }

void skinBrushTool::setExitToolCommand(MString value) { exitToolCommandVal = value; }

void skinBrushTool::setFractionOversampling(bool value) { fractionOversamplingVal = value; }

void skinBrushTool::setIgnoreLock(bool value) { ignoreLockVal = value; }

void skinBrushTool::setLineWidth(int value) { lineWidthVal = value; }

void skinBrushTool::setMessage(int value) { messageVal = value; }

void skinBrushTool::setOversampling(int value) { oversamplingVal = value; }

void skinBrushTool::setRange(double value) { rangeVal = value; }

void skinBrushTool::setSize(double value) { sizeVal = value; }

void skinBrushTool::setStrength(double value) { strengthVal = value; }

void skinBrushTool::setSmoothStrength(double value) { smoothStrengthVal = value; }

void skinBrushTool::setPruneWeights(double value) { pruneWeights = value; }

void skinBrushTool::setUndersampling(int value) { undersamplingVal = value; }

void skinBrushTool::setVolume(bool value) { volumeVal = value; }

void skinBrushTool::setCommandIndex(int value) { commandIndex = value; }

void skinBrushTool::setSmoothRepeat(int value) { smoothRepeat = value; }

void skinBrushTool::setUseColorSetsWhilePainting(bool value) { useColorSetsWhilePainting = value; }

void skinBrushTool::setDrawTriangles(bool value) { drawTriangles = value; }

void skinBrushTool::setDrawEdges(bool value) { drawEdges = value; }

void skinBrushTool::setDrawPoints(bool value) { drawPoints = value; }
void skinBrushTool::setDrawTransparency(bool value) { drawTransparency = value; }

void skinBrushTool::setSoloColorType(int value) { soloColorTypeVal = value; }

void skinBrushTool::setSoloColor(int value) { soloColorVal = value; }

void skinBrushTool::setCoverage(bool value) { coverageVal = value; }

void skinBrushTool::setPostSetting(bool value) { postSetting = value; }

// ---------------------------------------------------------------------
// public methods for setting the undo/redo variables
// ---------------------------------------------------------------------

void skinBrushTool::setInfluenceIndices(MIntArray indices) { influenceIndices = indices; }

void skinBrushTool::setInfluenceName(MString name) { influenceName = name; }

void skinBrushTool::setMesh(MDagPath dagPath) { meshDag = dagPath; }

void skinBrushTool::setNormalize(bool value) { normalize = value; }

void skinBrushTool::setSelection(MSelectionList selection, MSelectionList hilite) {
    undoSelection = selection;
    undoHilite = hilite;
}

void skinBrushTool::setSkinCluster(MObject skinCluster) { skinObj = skinCluster; }

void skinBrushTool::setVertexComponents(MObject components) { vertexComponents = components; }

void skinBrushTool::setWeights(MDoubleArray weights) { undoWeights = weights; }

void skinBrushTool::setUnoVertices(MIntArray editVertsIndices) { undoVertices = editVertsIndices; }

void skinBrushTool::setUnoLocks(MIntArray locks) { undoLocks = locks; }

void skinBrushTool::setRedoLocks(MIntArray locks) { redoLocks = locks; }
