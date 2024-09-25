#include "wireframeDisplay.h"

#include <maya/MFnMesh.h>
#include <maya/MFnMeshData.h>
#include <maya/MItMeshEdge.h>
#include <maya/MItMeshVertex.h>

#include "shapesDefinition.h"

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Node implementation with standard viewport draw
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

MObject wireframeDisplay::_inMesh;
// MObject wireframeDisplay::_inputColor;
MObject wireframeDisplay::_inputAlpha;
MObject wireframeDisplay::_lineWidth;
MObject wireframeDisplay::_enableSmooth;
MObject wireframeDisplay::_boundingBoxMini;
MObject wireframeDisplay::_boundingBoxMaxi;

MTypeId wireframeDisplay::id(0x001226F2);
MString wireframeDisplay::drawDbClassification("drawdb/geometry/wireframeDisplay");
MString wireframeDisplay::drawRegistrantId("wireframeDisplayNodePlugin");

wireframeDisplay::wireframeDisplay() : _update_attrs(true) {}
wireframeDisplay::~wireframeDisplay() {}

bool wireframeDisplay::drawLast() const { return true; }
bool wireframeDisplay::isTransparent() const { return true; }

void wireframeDisplay::postConstructor() {
    MObject self = thisMObject();
    MFnDependencyNode fn_node(self);
    fn_node.setName("wireframeDisplayShape#");

    _self = self;
    _update_attrs = true;
    MMatrix _transformMatrix;
}

void wireframeDisplayData::getPlugs(const MObject& node, bool getCol) {
    MStatus status;

    // get the mesh
    MPlug meshPlug = MPlug(node, wireframeDisplay::_inMesh);
    MPlugArray plugs;
    meshPlug.connectedTo(plugs, true, false, &status);

    if (plugs.length() == 0) {
    } else {
        this->inMesh = plugs[0].node();
        // get the color
    }
    MFnDagNode fn(node);
    if (getCol) {
        MPlug inputColPlug = fn.findPlug("overrideColorRGB", false);
        // MPlug inputColPlug = MPlug(node, MPxLocatorNode::overrideColorR);
        this->color[0] = inputColPlug.child(0).asFloat();
        this->color[1] = inputColPlug.child(1).asFloat();
        this->color[2] = inputColPlug.child(2).asFloat();
        this->color[3] = MPlug(node, wireframeDisplay::_inputAlpha).asFloat();
    } else {
        this->transparency = MPlug(node, wireframeDisplay::_inputAlpha).asFloat();
    }
    this->lineWidth = MPlug(node, wireframeDisplay::_lineWidth).asInt();
    this->enableSmooth = MPlug(node, wireframeDisplay::_enableSmooth).asBool();
}
MBoundingBox getBB(const MObject& node) {
    MStatus status;

    MPlugArray plugs;
    MPlug c1Plug(node, wireframeDisplay::_boundingBoxMini);
    MPlug c2Plug(node, wireframeDisplay::_boundingBoxMaxi);

    c1Plug.connectedTo(plugs, true, false, &status);
    if (plugs.length() == 0) {  // not connected boundingBox min
        MPlug meshPlug = MPlug(node, wireframeDisplay::_inMesh);
        meshPlug.connectedTo(plugs, true, false, &status);
        if (plugs.length() != 0) {  // connected inMesh

            MGlobal::displayInfo("doing boundingBox connection");
            MObject meshObj = plugs[0].node();
            MFnDependencyNode meshObjDep(meshObj, &status);
            if (MS::kSuccess != status) {
                MGlobal::displayError("MFnDependencyNode meshObjDep(meshObj, &status);");
            }

            MPlug meshBBMI = meshObjDep.findPlug("boundingBoxMin", false);
            MPlug meshBBMX = meshObjDep.findPlug("boundingBoxMax", false);

            MDGModifier dg;
            status = dg.connect(meshBBMI, c1Plug);
            if (MS::kSuccess != status) {
                MGlobal::displayError("connect ERROR " + meshBBMI.name() + " " + c1Plug.name());
            }
            status = dg.connect(meshBBMX, c2Plug);
            dg.doIt();
        }
    }
    double Xval1 = c1Plug.child(0).asDouble();
    double Yval1 = c1Plug.child(1).asDouble();
    double Zval1 = c1Plug.child(2).asDouble();
    MPoint corner1Point(Xval1, Yval1, Zval1);
    double Xval2 = c2Plug.child(0).asDouble();
    double Yval2 = c2Plug.child(1).asDouble();
    double Zval2 = c2Plug.child(2).asDouble();
    MPoint corner2Point(Xval2, Yval2, Zval2);

    return MBoundingBox(corner1Point, corner2Point);
}
void wireframeDisplayData::get(const MObject& node) {
    MStatus status;
    // get the wireframe
    this->edgeVertices.clear();
    this->theBoundingBox = MBoundingBox();
    if (this->inMesh != MObject::kNullObj) {
        MPoint pt0, pt1;

        MFnMesh tmpMesh(this->inMesh, &status);
        MFnMeshData meshData;
        MMeshSmoothOptions options;
        tmpMesh.getSmoothMeshDisplayOptions(options);
        this->theBoundingBox = tmpMesh.boundingBox(&status);

        bool recomputeEdgeAssociation = false;
        int currentNbEdges = tmpMesh.numEdges();
        if (currentNbEdges != this->nbEdges) {  // if mesh is updated with more edges
            MGlobal::displayInfo(MString("---- wireframeDisplayData::get - recompute edges : ") +
                                 this->nbEdges + MString(" - ") + currentNbEdges);

            this->edgeVerticesIndices.clear();
            recomputeEdgeAssociation = true;
            this->nbEdges = currentNbEdges;
        }
        int smoothLevel = 0;
        if (this->enableSmooth)
            smoothLevel = tmpMesh.findPlug("displaySmoothMesh", false, &status).asInt();
        if (smoothLevel > 0) {
            options.setDivisions(1);
            options.setSmoothUVs(false);
            // https://github.com/haggi/OpenMaya/blob/master/src/common/cpp/mayaObject.cpp

            MObject dataObject = meshData.create();
            MObject smoothedObj = tmpMesh.generateSmoothMesh(dataObject, &options, &status);
            MFnMesh smoothMesh(smoothedObj, &status);
            this->mayaRawPoints = smoothMesh.getRawPoints(&status);

            if (recomputeEdgeAssociation) {
                int nbVerts = tmpMesh.numVertices();
                MItMeshEdge edgeIter(smoothedObj);

                for (; !edgeIter.isDone(); edgeIter.next()) {
                    int pt0Index = edgeIter.index(0);
                    int pt1Index = edgeIter.index(1);
                    if (pt0Index < nbVerts || pt1Index < nbVerts) {
                        this->edgeVerticesIndices.append(pt0Index);
                        this->edgeVerticesIndices.append(pt1Index);
                    }
                }
            }
        } else {
            this->mayaRawPoints = tmpMesh.getRawPoints(&status);
            if (recomputeEdgeAssociation) {
                MItMeshEdge edgeIter(this->inMesh);
                for (; !edgeIter.isDone(); edgeIter.next()) {
                    int pt0Index = edgeIter.index(0);
                    int pt1Index = edgeIter.index(1);
                    this->edgeVerticesIndices.append(pt0Index);
                    this->edgeVerticesIndices.append(pt1Index);
                }
            }
        }
        if (recomputeEdgeAssociation) this->arrayLength = this->edgeVerticesIndices.length();
        this->edgeVertices.setLength(this->arrayLength);
        for (unsigned int i = 0; i < this->arrayLength; ++i) {
            int ptIndex = this->edgeVerticesIndices[i];
            float element[4] = {this->mayaRawPoints[ptIndex * 3],
                                this->mayaRawPoints[ptIndex * 3 + 1],
                                this->mayaRawPoints[ptIndex * 3 + 2], 0};
            this->edgeVertices.set(element, i);
        }
    }
}

MStatus wireframeDisplay::compute(const MPlug& plug, MDataBlock& data) {
    return MS::kSuccess;
}

#ifdef MAYA_LEGACY_DISPLAY
// called by legacy default viewport
void wireframeDisplay::draw(M3dView& view, const MDagPath& path, M3dView::DisplayStyle style,
                            M3dView::DisplayStatus status) {
    wireframeDisplayData data;
    data.getPlugs(_self, true);
    data.get(_self);

    glPushAttrib(GL_CURRENT_BIT);

    // Draw the wireframe
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glDepthMask(5);
    glPushMatrix();

    glLineWidth(data.lineWidth);

    glBegin(GL_LINES);

    if (status == M3dView::kLead) {
        glColor4f(0., 1., 0., data.color[3]);  // green
    } else if (status == M3dView::kActive) {
        glColor4f(1., 1., 1., data.color[3]);  // white
    } else {
        glColor4fv(data.color);
    }

    int nbEdges = data.edgeVertices.length();
    int indexPt;
    for (int i = 0; i < nbEdges; i += 2) {
        glVertex3f(data.edgeVertices[i].x, data.edgeVertices[i].y, data.edgeVertices[i].z);
        glVertex3f(data.edgeVertices[i + 1].x, data.edgeVertices[i + 1].y,
                   data.edgeVertices[i + 1].z);
    }
    glEnd();
    glLineWidth(1);

    view.endGL();

    glDisable(GL_BLEND);

    // re-enable depth writes
    glDepthMask(1);
    // restore the old openGL settings
    glPopAttrib();
    // Draw the name of the wireframeDisplay
}
#endif

bool wireframeDisplay::isBounded() const { return true; }

MBoundingBox wireframeDisplay::boundingBox() const {
    // Get the size
    //
    MObject thisNode = thisMObject();

    return getBB(_self);
    // data.get(thisMObject(), _transformMatrix);
    // return data.theBoundingBox;
}

void* wireframeDisplay::creator() { return new wireframeDisplay(); }

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Viewport 2.0 override implementation
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

// By setting isAlwaysDirty to false in MPxDrawOverride constructor, the
// draw override will be updated (via prepareForDraw()) only when the node
// is marked dirty via DG evaluation or dirty propagation. Additional
// callback is also added to explicitly mark the node as being dirty (via
// MRenderer::setGeometryDrawDirty()) for certain circumstances. Note that
// the draw callback in MPxDrawOverride constructor is set to NULL in order
// to achieve better performance.
wireframeDisplayDrawOverride::wireframeDisplayDrawOverride(const MObject& obj)
    : MHWRender::MPxDrawOverride(obj, NULL, false) {
    fModelEditorChangedCbId =
        MEventMessage::addEventCallback("modelEditorChanged", OnModelEditorChanged, this);

    MStatus status;
    MFnDependencyNode node(obj, &status);
    fwireframeDisplay = status ? dynamic_cast<wireframeDisplay*>(node.userNode()) : NULL;
}

wireframeDisplayDrawOverride::~wireframeDisplayDrawOverride() {
    fwireframeDisplay = NULL;

    if (fModelEditorChangedCbId != 0) {
        MMessage::removeCallback(fModelEditorChangedCbId);
        fModelEditorChangedCbId = 0;
    }
}

void wireframeDisplayDrawOverride::OnModelEditorChanged(void* clientData) {
    // Mark the node as being dirty so that it can update on display appearance
    // switch among wireframe and shaded.
    wireframeDisplayDrawOverride* ovr = static_cast<wireframeDisplayDrawOverride*>(clientData);
    if (ovr && ovr->fwireframeDisplay) {
        MHWRender::MRenderer::setGeometryDrawDirty(ovr->fwireframeDisplay->thisMObject());
    }
}

MHWRender::DrawAPI wireframeDisplayDrawOverride::supportedDrawAPIs() const {
    // this plugin supports both GL and DX
    return (MHWRender::kOpenGL | MHWRender::kDirectX11 | MHWRender::kOpenGLCoreProfile);
}

bool wireframeDisplayDrawOverride::isBounded(const MDagPath& /*objPath*/,
                                             const MDagPath& /*cameraPath*/) const {
    return true;
}

MBoundingBox wireframeDisplayDrawOverride::boundingBox(const MDagPath& objPath,
                                                       const MDagPath& cameraPath) const {
    MObject node = objPath.node();
    return getBB(node);

}

// Called by Maya each time the object needs to be drawn.
MUserData* wireframeDisplayDrawOverride::prepareForDraw(
    const MDagPath& objPath, const MDagPath& cameraPath,
    const MHWRender::MFrameContext& frameContext, MUserData* oldData) {
    // Any data needed from the Maya dependency graph must be retrieved and cached in this stage.
    // There is one cache data for each drawable instance, if it is not desirable to allow Maya to
    // handle data caching, simply return null in this method and ignore user data parameter in draw
    // callback method. e.g. in this sample, we compute and cache the data for usage later when we
    // create the MUIDrawManager to draw wireframeDisplay in method addUIDrawables().
    MStatus stat;
    wireframeDisplayData* data = dynamic_cast<wireframeDisplayData*>(oldData);
    MObject node = objPath.node(&stat);

    if (!data) {
        data = new wireframeDisplayData;
    }
    data->getPlugs(node, false);
    data->get(node);

    // get correct color based on the state of object, e.g. active or dormant
    // kLead
    data->displayStat = MHWRender::MGeometryUtilities::displayStatus(objPath);
    // http://help.autodesk.com/view/MAYAUL/2016/ENU/?guid=__cpp_ref_class_m3d_view_html
    // data->displayStat = displayStat;
    data->fColor = MHWRender::MGeometryUtilities::wireframeColor(objPath);
    return data;
}

// addUIDrawables() provides access to the MUIDrawManager, which can be used
// to queue up operations for drawing simple UI elements such as lines, circles and
// text. To enable addUIDrawables(), override hasUIDrawables() and make it return true.
void wireframeDisplayDrawOverride::addUIDrawables(const MDagPath& objPath,
                                                  MHWRender::MUIDrawManager& drawManager,
                                                  const MHWRender::MFrameContext& frameContext,
                                                  const MUserData* data) {
    // Get data cached by prepareForDraw() for each drawable instance, then MUIDrawManager
    // can draw simple UI by these data.
    wireframeDisplayData* pLocatorData = (wireframeDisplayData*)data;
    if (!pLocatorData) {
        return;
    }

    drawManager.beginDrawable(MHWRender::MUIDrawManager::kNonSelectable);  // so not selectable
    if (pLocatorData->displayStat == MHWRender::kLead) {
        MColor color(0., 1., 0., pLocatorData->transparency);
        drawManager.setColor(color);
    } else if (pLocatorData->displayStat == MHWRender::kActive) {
        MColor color(1., 1., 1., pLocatorData->transparency);
        drawManager.setColor(color);
    } else {
        MColor color(pLocatorData->fColor.r, pLocatorData->fColor.g, pLocatorData->fColor.b,
                     pLocatorData->transparency);
        drawManager.setColor(color);
    }

    drawManager.setLineWidth(pLocatorData->lineWidth);
    drawManager.setDepthPriority(5);

    // Draw the wireframe
    drawManager.lineList(pLocatorData->edgeVertices, false);

    drawManager.endDrawable();
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Plugin initialize
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

MStatus wireframeDisplay::initialize() {
    MFnUnitAttribute unitFn;
    MFnNumericAttribute nAttr;
    MFnEnumAttribute enumAttr;

    MStatus stat;

    MFnTypedAttribute inMeshAttrFn;
    _inMesh = inMeshAttrFn.create("inMesh", "inMesh", MFnData::kMesh);
    CHECK_MSTATUS(inMeshAttrFn.setStorable(true));
    CHECK_MSTATUS(inMeshAttrFn.setKeyable(false));
    CHECK_MSTATUS(inMeshAttrFn.setReadable(true));
    CHECK_MSTATUS(inMeshAttrFn.setWritable(true));
    CHECK_MSTATUS(inMeshAttrFn.setCached(false));
    CHECK_MSTATUS(addAttribute(_inMesh));

    _inputAlpha = nAttr.create("inputAlpha", "ina", MFnNumericData::kFloat, 0.5);
    nAttr.setMin(0.0);
    nAttr.setMax(1.0);
    nAttr.setKeyable(true);
    nAttr.setStorable(true);
    nAttr.setReadable(true);
    nAttr.setWritable(true);
    nAttr.setChannelBox(false);

    // add the color attribute to our node
    stat = addAttribute(_inputAlpha);

    _lineWidth = nAttr.create("lineWidth", "lw", MFnNumericData::kInt, 1);
    nAttr.setMin(1);
    nAttr.setMax(10);
    nAttr.setKeyable(true);
    nAttr.setStorable(true);
    nAttr.setReadable(true);
    nAttr.setWritable(true);
    nAttr.setChannelBox(false);

    // add the color attribute to our node
    stat = addAttribute(_lineWidth);

    _enableSmooth = nAttr.create("enableSmooth", "es", MFnNumericData::kBoolean, false);
    nAttr.setKeyable(true);
    nAttr.setStorable(true);
    nAttr.setReadable(true);
    nAttr.setWritable(true);
    nAttr.setChannelBox(false);

    // add the color attribute to our node
    stat = addAttribute(_enableSmooth);

    // to compute the bounding box
    _boundingBoxMini = nAttr.createPoint("boundingBoxMini", "bbmini");
    nAttr.setDefault(0., 0., 0.);
    nAttr.setStorable(true);
    nAttr.setKeyable(true);
    nAttr.setWritable(true);
    stat = addAttribute(_boundingBoxMini);

    _boundingBoxMaxi = nAttr.createPoint("boundingBoxMaxi", "bbmaxi");
    nAttr.setDefault(0., 0., 0.);
    nAttr.setStorable(true);
    nAttr.setKeyable(true);
    nAttr.setWritable(true);
    stat = addAttribute(_boundingBoxMaxi);

    return MS::kSuccess;
}
