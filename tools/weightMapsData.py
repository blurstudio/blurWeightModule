from maya import OpenMayaUI, OpenMaya, OpenMayaAnim
import maya.api.OpenMaya as OpenMaya2

from maya import cmds, mel
from functools import partial

# import shiboken2 as shiboken
import time, datetime

from ctypes import c_double, c_float

import numpy as np
import re
from utils import GlobalContext, getSoftSelectionValuesNEW, getThreeIndices

from .abstractData import DataAbstract, isin

"""
cmds.getAttr("blendShape1.inputTarget[0].baseWeights") [0]
cmds.getAttr("blendShape1.inputTarget[0].baseWeights", mi = True)

values = [.11,.12,.13]
cmds.setAttr("blendShape1.inputTarget[0].baseWeights[0:2]",*values, size=len(values))
"""


class DataOfOneDimensionalAttrs(DataAbstract):
    useAPI = False  # for setting values use API

    def __init__(
        self,
        useShortestNames=False,
        hideZeroColumn=True,
        createDisplayLocator=True,
        mainWindow=None,
    ):
        self.useShortestNames = useShortestNames
        self.hideZeroColumn = hideZeroColumn
        self.clearData()
        super(DataOfOneDimensionalAttrs, self).__init__(
            createDisplayLocator=createDisplayLocator, mainWindow=mainWindow
        )

    # -----------------------------------------------------------------------------------------------------------
    # export import  -------------------------------------------------------------------------------------------
    # -----------------------------------------------------------------------------------------------------------
    def exportColumns(self, colIndices):
        """
        print colIndices
        print [self.listAttrs[i] for i in colIndices]
        print [self.shortColumnsNames[i] for i in colIndices]
        """
        # 1 re-get the values
        self.getAttributesValues(onlyfullArr=True)
        # 2 subArray:
        sceneName = cmds.file(q=True, sceneName=True)
        splt = sceneName.split("/")
        startDir = "/".join(splt[:-1])
        res = cmds.fileDialog2(
            fileMode=3, dialogStyle=1, caption="save data", startingDirectory=startDir
        )
        if res:
            destinationFolder = res.pop()
            for ind in colIndices:
                filePth = "{}/{}.gz".format(destinationFolder, self.shortColumnsNames[ind])
                print filePth
                arrToExport = np.copy(self.fullAttributesArr[:, ind])
                np.savetxt(filePth, arrToExport)

    def importColumns(self, colIndices):
        """
        print colIndices
        print [self.listAttrs[i] for i in colIndices]
        print [self.shortColumnsNames[i] for i in colIndices]
        """
        # 2 subArray:
        sceneName = cmds.file(q=True, sceneName=True)
        splt = sceneName.split("/")
        startDir = "/".join(splt[:-1])
        res = cmds.fileDialog2(
            fileMode=4, dialogStyle=1, caption="save data", startingDirectory=startDir
        )
        if res:
            if len(res) == 1:
                (filePth,) = res
                for colIndex in colIndices:
                    self.doImport(filePth, colIndex)
                return None
            else:
                return [self.shortColumnsNames[i] for i in colIndices], res
        return None

    def doImport(self, filePth, colIndex):
        print filePth
        fileArr = np.loadtxt(str(filePth))
        difference = fileArr - self.fullAttributesArr[:, colIndex]

        indicesDifferents = np.nonzero(difference)
        values = fileArr[indicesDifferents]

        vertsIndicesWeights = zip(indicesDifferents[0].tolist(), values.tolist())
        self.setAttributeValues(self.listAttrs[colIndex], vertsIndicesWeights)

    # -----------------------------------------------------------------------------------------------------------
    # Attrs functions -------------------------------------------------------------------------------------
    # -----------------------------------------------------------------------------------------------------------
    def getListPaintableAttributes(self, theNodeShape):
        listDeformersTypes = cmds.nodeType("geometryFilter", derived=True, isTypeName=True)
        listShapesTypes = cmds.nodeType("shape", derived=True, isTypeName=True)

        paintableItems = cmds.artBuildPaintMenu(theNodeShape).split(" ")

        lstDeformers = []
        lstShapes = []
        lstOthers = []
        blendShapes = set()

        self.dicDisplayNames = {}
        self.attributesToPaint = {}
        toSel = ""
        for itemToPaint in paintableItems:
            if not itemToPaint:
                continue
            splt = itemToPaint.split(".")
            nodeType, nodeName, attr = splt[:3]
            nodeNameShort = nodeName.split("|")[-1]
            displayName = "-".join([nodeNameShort, attr])
            if not cmds.attributeQuery(attr, node=nodeName, ex=True):
                continue
            if nodeType == "skinCluster":
                toSel = displayName
                continue
            if nodeType == "blendShape":
                blendShapes.add(nodeName)
                continue
            self.dicDisplayNames[displayName] = nodeName + "." + attr
            self.attributesToPaint[displayName] = itemToPaint[:-2]

            if nodeType in listDeformersTypes:
                lstDeformers.append(displayName)
            elif nodeType in listShapesTypes:
                lstShapes.append(displayName)
            else:
                lstOthers.append(displayName)
        return lstDeformers, lstOthers, lstShapes

    def getAttributesValues(self, indices=[], onlyfullArr=False):
        with GlobalContext(message="getAttributesValues", doPrint=self.verbose):
            nbAttrs = len(self.listAttrs)
            # initialize array at 1.0
            self.fullAttributesArr = np.full((self.nbVertices, nbAttrs), 1.0)
            for indAtt, att in enumerate(self.listAttrs):
                # print att
                indicesAtt = cmds.getAttr(att, mi=True)
                if indicesAtt:
                    values = cmds.getAttr(att)[0]
                    self.fullAttributesArr[indicesAtt, indAtt] = values
            if onlyfullArr:
                return
            # self.printArrayData(self.fullAttributesArr)
            if indices:
                if self.softOn:
                    revertSortedIndices = np.array(indices)[self.opposite_sortedIndices]
                else:
                    revertSortedIndices = indices
                self.raw2dArray = self.fullAttributesArr[
                    revertSortedIndices,
                ]
            else:
                self.raw2dArray = self.fullAttributesArr
            # self.printArrayData(self.raw2dArray)
            # ---- reorder --------------------------------------------
            if self.softOn:  # order with indices
                self.display2dArray = self.raw2dArray[self.sortedIndices]
            else:
                self.display2dArray = self.raw2dArray

    def setValueInDeformer(self, arrayForSetting):
        # self.printArrayData(arrayForSetting)
        arrIndicesVerts = np.array(self.vertices)
        editedColumns = np.any(self.sumMasks, axis=0).tolist()
        rows = arrayForSetting.shape[0]
        attsValues = []
        if self.storeUndo:
            undoValues = []
        for colIndex, isColumnChanged in enumerate(editedColumns):
            if isColumnChanged:
                # we can also check what didn't change with a difference same as in doImport
                indices = np.nonzero(self.sumMasks[:, colIndex])[0]
                values = arrayForSetting[indices, colIndex]
                verts = arrIndicesVerts[indices + self.Mtop]
                vertsIndicesWeights = zip(verts.tolist(), values.tolist())

                # self.setAttributeValues(self.listAttrs [colIndex],vertsIndicesWeights)
                attsValues.append((self.listAttrs[colIndex], vertsIndicesWeights))
                # now the undo values ------------------------------
                if self.storeUndo:
                    valuesOrig = self.fullAttributesArr[verts.tolist(), colIndex]
                    undoVertsIndicesWeights = zip(verts.tolist(), valuesOrig.tolist())
                    undoValues.append((self.listAttrs[colIndex], undoVertsIndicesWeights))
        if self.storeUndo:
            self.undoValues = undoValues
            self.storeUndo = False
        self.redoValues = attsValues
        self.setAttsValues(attsValues)

    def setAttsValues(self, attsValues):
        # stor undo values and redo values
        for att, vertsIndicesWeights in attsValues:
            self.setAttributeValues(att, vertsIndicesWeights)

    def setAttributeValues(self, att, vertsIndicesWeights):
        if not vertsIndicesWeights:
            return
        if self.useAPI:
            MSel = OpenMaya2.MSelectionList()
            MSel.add(att)

            plg2 = MSel.getPlug(0)
            # ids = plg2.getExistingArrayAttributeIndices()
            # count = len(ids)
            with GlobalContext():
                for indVtx, value in vertsIndicesWeights:
                    plg2.elementByLogicalIndex(indVtx).setFloat(value)
            # elementByLogicalIndex  faster than elementByPhysicalIndex
        else:
            # need an undo Context
            listMelValueWeights = self.orderMelListValues(vertsIndicesWeights)
            # print listMelValueWeights

            for indices, weightArray in listMelValueWeights:
                if isinstance(weightArray, list):
                    start, finish = indices
                    length = len(weightArray)
                    cmds.setAttr(att + "[{0}:{1}]".format(start, finish), *weightArray, size=length)
                else:
                    index, value = indices, weightArray
                    cmds.setAttr(att + "[{}]".format(index), value)

    def smoothVertices(self, iteration=10):
        # print "iteration", iteration
        self.getAttributesValues(onlyfullArr=True)

        arrIndicesVerts = np.array(self.vertices)

        # for the extended neighBoors
        padder = range(self.maxNeighboors)
        dicOfVertsSubArray = {}
        attsValues = []
        if self.storeUndo:
            undoValues = []
        with GlobalContext(message="smoothVertices", doPrint=True):
            new2dArray = np.copy(self.orig2dArray)

            editedColumns = np.any(self.sumMasks, axis=0).tolist()
            rows = new2dArray.shape[0]
            for colIndex, isColumnChanged in enumerate(editedColumns):
                if isColumnChanged:
                    # get indices to set ---------------------------------------
                    indices = np.nonzero(self.sumMasks[:, colIndex])[0]
                    # values  = new2dArray [ indices, colIndex]
                    # get vertices to set ------------------------------------
                    verts = arrIndicesVerts[indices + self.Mtop]

                    # prepare array for mean -----------------------------------
                    nbNonZero = np.count_nonzero(self.sumMasks[:, colIndex])
                    arrayForMean = np.full((nbNonZero, self.maxNeighboors), 0)
                    arrayForMeanMask = np.full((nbNonZero, self.maxNeighboors), False, dtype=bool)
                    if self.storeUndo:
                        valuesOrig = self.fullAttributesArr[verts.tolist(), colIndex]
                        undoVertsIndicesWeights = zip(verts.tolist(), valuesOrig.tolist())
                        undoValues.append((self.listAttrs[colIndex], undoVertsIndicesWeights))
                    for _ in xrange(iteration):
                        for i, vertIndex in enumerate(verts):
                            if vertIndex not in dicOfVertsSubArray:
                                # print vertIndex
                                connectedVertices = self.vertNeighboors[vertIndex]
                                connectedVerticesExtended = (connectedVertices + padder)[
                                    : self.maxNeighboors
                                ]
                                dicOfVertsSubArray[vertIndex] = connectedVerticesExtended

                                arrayForMeanMask[i, 0 : self.nbNeighBoors[vertIndex]] = True
                            else:
                                connectedVerticesExtended = dicOfVertsSubArray[vertIndex]
                            # subArr = self.fullAttributesArr[connectedVertices, colIndex]
                            # arrayForMean[i, 0:self.nbNeighBoors[vertIndex]] = subArr
                            arrayForMean[i] = self.fullAttributesArr[
                                connectedVerticesExtended, colIndex
                            ]
                        meanCopy = np.ma.array(arrayForMean, mask=~arrayForMeanMask, fill_value=0)
                        meanValues = np.ma.mean(meanCopy, axis=1)
                        # update array:
                        self.fullAttributesArr[verts, colIndex] = meanValues
                    vertsIndicesWeights = zip(verts.tolist(), meanValues.tolist())
                    attsValues.append((self.listAttrs[colIndex], vertsIndicesWeights))
            if self.storeUndo:
                self.undoValues = undoValues
                self.storeUndo = False
            self.redoValues = attsValues
            self.setAttsValues(attsValues)

    # -----------------------------------------------------------------------------------------------------------
    # redefine abstract data functions -------------------------------------------------------------------------
    # -----------------------------------------------------------------------------------------------------------
    def postGetData(
        self, displayLocator=True, force=True, inputVertices=None, prevDeformedShape=""
    ):
        if displayLocator:
            self.connectDisplayLocator()
        self.getSoftSelectionVertices(inputVertices=inputVertices)

        if not self.vertices:
            self.vertices = range(self.nbVertices)
            self.verticesWeight = [1.0] * len(self.vertices)
            self.sortedIndices = range(len(self.vertices))
            self.opposite_sortedIndices = range(len(self.vertices))
            self.softOn = 0
            self.fullShapeIsUsed = True
        else:
            self.fullShapeIsUsed = False
        # get blendShapes weights values
        if self.vertices:
            self.getAttributesValues(indices=self.vertices)
        else:
            self.getAttributesValues()

        self.createRowText()
        self.rowCount = len(self.vertices)  # self.nbVertices
        self.columnCount = len(self.listAttrs)

        self.getLocksInfo()
        if force or prevDeformedShape != self.deformedShape:
            self.getConnectVertices()
        return True

    def clearData(self):
        super(DataOfOneDimensionalAttrs, self).clearData()
        self.BSnode = ""
        self.listAttrShortName, self.listAttrs = [], []
        self.fullAttributesArr = []

        self.dicDisplayNames = {}
        self.attributesToPaint = {}

    preSel = ""


#########################################################################################################
######### BlendShape ####################################################################################
#########################################################################################################
class DataOfBlendShape(DataOfOneDimensionalAttrs):
    # -----------------------------------------------------------------------------------------------------------
    # blendShape functions -------------------------------------------------------------------------------------
    # -----------------------------------------------------------------------------------------------------------
    def getBlendShapesAttributes(self, BSnode, theNodeShape):
        with GlobalContext(message="getBlendShapesAttributes", doPrint=False):
            lsGeomsOrig = cmds.blendShape(BSnode, q=True, geometry=True)
            lsGeomsIndicesOrig = cmds.blendShape(BSnode, q=True, geometryIndices=True)

            listAttrs = []
            listAttrShortName = []
            if theNodeShape in lsGeomsOrig:
                # get the index of the node in the blendShape
                inputTarget = lsGeomsIndicesOrig[lsGeomsOrig.index(theNodeShape)]

                listAttrShortName.append("baseWeights")
                listAttrs.append("{}.inputTarget[{}].baseWeights".format(BSnode, inputTarget))

                # get the alias -------------------------------------------------------
                listAlias = cmds.aliasAttr(BSnode, q=True)
                listAliasIndices = cmds.getAttr(
                    BSnode + ".inputTarget[{}].inputTargetGroup".format(inputTarget), mi=True
                )

                listAliasNme = (
                    zip(listAlias[0::2], listAlias[1::2])
                    if listAlias
                    else [
                        ("targetWeights_{}".format(i), "weight[{}]".format(i))
                        for i in listAliasIndices
                    ]
                )
                dicIndex = {}
                for el, wght in listAliasNme:
                    dicIndex[int(re.findall(r"\b\d+\b", wght)[0])] = el
                # end alias -------------------------------------------------------------

                for channelIndex in listAliasIndices:
                    attrShortName = dicIndex[channelIndex]
                    attr = "{}.inputTarget[{}].inputTargetGroup[{}].targetWeights".format(
                        BSnode, inputTarget, channelIndex
                    )

                    listAttrShortName.append(attrShortName)
                    listAttrs.append(attr)
                # for paintable --------------
                for shortName in listAttrShortName:
                    self.attributesToPaint[shortName] = "blendShape.{}.baseWeights".format(BSnode)
                return listAttrShortName, listAttrs
            else:
                return [], []

    # -----------------------------------------------------------------------------------------------------------
    # redefine abstract data functions -------------------------------------------------------------------------
    # -----------------------------------------------------------------------------------------------------------
    def getAllData(self, displayLocator=True, force=True, inputVertices=None):
        with GlobalContext(message="getAllData BlendShapes", doPrint=self.verbose):
            prevDeformedShape = self.deformedShape

            success = self.getDataFromSelection(
                typeOfDeformer="blendShape", force=force, inputVertices=inputVertices
            )
            if not success or self.theDeformer == "":
                return False
            else:
                self.BSnode = self.theDeformer

            # print self.BSnode
            self.getShapeInfo()
            # get list belndShapes attributes
            self.columnsNames, self.listAttrs = self.getBlendShapesAttributes(
                self.BSnode, self.deformedShape
            )
            self.shortColumnsNames = self.columnsNames

            return self.postGetData(
                displayLocator=displayLocator,
                force=force,
                inputVertices=inputVertices,
                prevDeformedShape=prevDeformedShape,
            )


class DataOfDeformers(DataOfOneDimensionalAttrs):
    def getDeformersAttributes(self):
        lstDeformers, lstOthers, lstShapes = self.getListPaintableAttributes(self.deformedShape)
        # get the index of the shape in the deformer !
        listAttrs = []
        lstDeformersRtn = []
        for dfmNm in lstDeformers:
            dfm, attName = dfmNm.split("-")
            if cmds.attributeQuery(attName, node=dfm, ex=True):
                lstDeformersRtn.append(dfmNm)
                isMulti = cmds.attributeQuery(attName, node=dfm, multi=True)
                # if attName == "weights":
                if isMulti:
                    # print dfm, attName
                    lsGeomsOrig = cmds.deformer(dfm, q=True, geometry=True)
                    lsGeomsIndicesOrig = cmds.deformer(dfm, q=True, geometryIndices=True)
                    if self.deformedShape in lsGeomsOrig:
                        inputTarget = lsGeomsIndicesOrig[lsGeomsOrig.index(self.deformedShape)]
                    else:
                        inputTarget = 0
                    prtAtt = cmds.attributeQuery(attName, node=dfm, listParent=True)
                    prtAtt = ".".join(prtAtt)
                    theAtt = "{}.{}[{}].{}".format(dfm, prtAtt, inputTarget, attName)
                    listAttrs.append(theAtt)
                else:
                    listAttrs.append(self.dicDisplayNames[dfmNm])
        # listAttrs = [self.dicDisplayNames [el].replace(".weights",".weightList[0].weights" ) for el in lstDeformers]
        return lstDeformersRtn, listAttrs

    # -----------------------------------------------------------------------------------------------------------
    # redefine abstract data functions -------------------------------------------------------------------------
    # -----------------------------------------------------------------------------------------------------------
    def getAllData(self, displayLocator=True, force=True, inputVertices=None, **kwargs):
        prevDeformedShape = self.deformedShape

        success = self.getDataFromSelection(
            typeOfDeformer=None, force=force, inputVertices=inputVertices, **kwargs
        )
        if not success:
            return False
        self.getShapeInfo()

        # get list deformers attributes
        self.columnsNames, self.listAttrs = self.getDeformersAttributes()
        self.shortColumnsNames = self.columnsNames
        # print self.shortColumnsNames , self.listAttrs

        return self.postGetData(
            displayLocator=displayLocator,
            force=force,
            inputVertices=inputVertices,
            prevDeformedShape=prevDeformedShape,
        )
