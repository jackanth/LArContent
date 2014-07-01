/**
 *  @file   LArContent/src/LArThreeDReco/LArCosmicRay/DeltaRayIdentificationAlgorithm.cc
 *
 *  @brief  Implementation of the delta ray identification algorithm class.
 *
 *  $Log: $
 */

#include "Pandora/AlgorithmHeaders.h"

#include "LArHelpers/LArClusterHelper.h"
#include "LArHelpers/LArPfoHelper.h"

#include "LArThreeDReco/LArCosmicRay/DeltaRayIdentificationAlgorithm.h"

using namespace pandora;

namespace lar
{

StatusCode DeltaRayIdentificationAlgorithm::Run()
{
    PfoList parentPfos, daughterPfos;
    this->GetPfos(m_parentPfoListName, parentPfos);
    this->GetPfos(m_daughterPfoListName, daughterPfos);

    if (parentPfos.empty())
    {
        std::cout << "DeltaRayIdentificationAlgorithm: could not find pfo list " << m_parentPfoListName << std::endl;
        return STATUS_CODE_SUCCESS;
    }

    // Build parent/daughter associations (currently using length and proximity)
    PfoAssociationMap pfoAssociationMap;
    this->BuildAssociationMap(parentPfos, daughterPfos, pfoAssociationMap);

    // Create the parent/daughter links
    PfoList newDaughterPfoList;
    this->BuildParentDaughterLinks(pfoAssociationMap, newDaughterPfoList);

    if (!newDaughterPfoList.empty())
    {
        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::SaveList(*this, m_parentPfoListName, m_daughterPfoListName,
            newDaughterPfoList));
    }

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

void DeltaRayIdentificationAlgorithm::GetPfos(const std::string inputPfoListName, PfoList &outputPfoList) const
{
    const PfoList *pPfoList = NULL;
    PANDORA_THROW_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_INITIALIZED, !=, PandoraContentApi::GetList(*this,
        inputPfoListName, pPfoList));

    if (NULL == pPfoList)
        return;

    outputPfoList.insert(pPfoList->begin(), pPfoList->end());
}

//------------------------------------------------------------------------------------------------------------------------------------------

void DeltaRayIdentificationAlgorithm::BuildAssociationMap(const PfoList &parentPfos, const PfoList &daughterPfos,
    PfoAssociationMap &pfoAssociationMap) const
{
    PfoList allPfos;
    allPfos.insert(parentPfos.begin(), parentPfos.end());
    allPfos.insert(daughterPfos.begin(), daughterPfos.end());

    // Loop over possible daughter Pfos in primary list
    for (PfoList::const_iterator iter1 = parentPfos.begin(), iterEnd1 = parentPfos.end(); iter1 != iterEnd1; ++iter1)
    {
        const ParticleFlowObject *pDaughterPfo = *iter1;

        // Find the best parent Pfo using combined list
        ParticleFlowObject *pBestParentPfo = NULL;
        float bestDisplacementSquared(std::numeric_limits<float>::max());

        for (PfoList::const_iterator iter2 = allPfos.begin(), iterEnd2 = allPfos.end(); iter2 != iterEnd2; ++iter2)
        {
            ParticleFlowObject *pThisParentPfo = *iter2;
            float thisDisplacementSquared(std::numeric_limits<float>::max());

            if (pDaughterPfo == pThisParentPfo)
                continue;

            if (!this->IsAssociated(pDaughterPfo, pThisParentPfo, thisDisplacementSquared))
                continue;

            if (thisDisplacementSquared < bestDisplacementSquared)
            {
                bestDisplacementSquared = thisDisplacementSquared;
                pBestParentPfo = pThisParentPfo;
            }
        }

        if (!pBestParentPfo)
            continue;

        // Case 1: candidate parent comes from primary list
        if (pBestParentPfo->GetParentPfoList().empty())
        {
            // Check: parent shouldn't live in the secondary list
            if (daughterPfos.count(pBestParentPfo))
                throw StatusCodeException(STATUS_CODE_FAILURE);

            pfoAssociationMap.insert(PfoAssociationMap::value_type(pDaughterPfo, pBestParentPfo));
        }

        // Case 2: candidate parent comes from secondary list
        else
        {
            // Check: parent shouldn't live in the primary list
            if (parentPfos.count(pBestParentPfo))
                throw StatusCodeException(STATUS_CODE_FAILURE);

            // Check: there should only be one parent
            if (pBestParentPfo->GetParentPfoList().size() != 1)
                throw StatusCodeException(STATUS_CODE_FAILURE);

            // Check: get the new parent (and check there is no grand-parent)
            PfoList::iterator pIter = pBestParentPfo->GetParentPfoList().begin();
            ParticleFlowObject *pReplacementParentPfo = *pIter;
            if (pReplacementParentPfo->GetParentPfoList().size() != 0)
                throw StatusCodeException(STATUS_CODE_FAILURE);

            pfoAssociationMap.insert(PfoAssociationMap::value_type(pDaughterPfo, pReplacementParentPfo));
        }

    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

bool DeltaRayIdentificationAlgorithm::IsAssociated(const ParticleFlowObject *const pDaughterPfo, const ParticleFlowObject *const pParentPfo,
    float &displacementSquared) const
{
    displacementSquared = std::numeric_limits<float>::max();

    if (pDaughterPfo == pParentPfo)
        return false;

    const float daughterLengthSquared(LArPfoHelper::GetTwoDLengthSquared(pDaughterPfo));
    const float parentLengthSquared(LArPfoHelper::GetTwoDLengthSquared(pParentPfo));
    
    if (daughterLengthSquared > m_maxDaughterLengthSquared || parentLengthSquared < m_minParentLengthSquared ||
        daughterLengthSquared > 0.5 * parentLengthSquared)
        return false;

    const float transitionLengthSquared(100.f);
    const float displacementCut((daughterLengthSquared > transitionLengthSquared) ? m_distanceForMatching : 
        m_distanceForMatching * (2.f - daughterLengthSquared / transitionLengthSquared));
    const float displacementCutSquared(displacementCut * displacementCut);

    try
    {
        displacementSquared = this->GetDisplacementSquared(pDaughterPfo, pParentPfo);
    }
    catch(StatusCodeException &statusCodeException)
    {
        if (STATUS_CODE_FAILURE == statusCodeException.GetStatusCode())
            throw statusCodeException;
    }

    if (displacementSquared > displacementCutSquared)
        return false;

    return true;
}

//------------------------------------------------------------------------------------------------------------------------------------------

float DeltaRayIdentificationAlgorithm::GetDisplacementSquared(const ParticleFlowObject *const pDaughterPfo, const ParticleFlowObject *const pParentPfo) const
{
    CartesianPointList vertexListU, vertexListV, vertexListW;
    this->GetTwoDVertexList(pDaughterPfo, TPC_VIEW_U, vertexListU);
    this->GetTwoDVertexList(pDaughterPfo, TPC_VIEW_V, vertexListV);
    this->GetTwoDVertexList(pDaughterPfo, TPC_VIEW_W, vertexListW);

    ClusterVector clusterVectorU, clusterVectorV, clusterVectorW;
    LArPfoHelper::GetClusters(pParentPfo, TPC_VIEW_U, clusterVectorU);
    LArPfoHelper::GetClusters(pParentPfo, TPC_VIEW_V, clusterVectorV);
    LArPfoHelper::GetClusters(pParentPfo, TPC_VIEW_W, clusterVectorW);

    float sumViews(0.f);
    float sumDisplacementSquared(0.f);

    if (!vertexListU.empty())
    {
        sumDisplacementSquared += this->GetTwoDSeparation(vertexListU, clusterVectorU);
        sumViews += 1.f;
    }

    if (!vertexListV.empty())
    {
        sumDisplacementSquared += this->GetTwoDSeparation(vertexListV, clusterVectorV);
        sumViews += 1.f;
    } 

    if (!vertexListW.empty())
    {
        sumDisplacementSquared += this->GetTwoDSeparation(vertexListW, clusterVectorW);
        sumViews += 1.f;
    }

    if (sumViews < std::numeric_limits<float>::epsilon())
        throw StatusCodeException(STATUS_CODE_FAILURE);
        
    return std::sqrt(sumDisplacementSquared / sumViews);
}

//------------------------------------------------------------------------------------------------------------------------------------------

void DeltaRayIdentificationAlgorithm::GetTwoDVertexList(const ParticleFlowObject *const pPfo, const HitType &hitType, CartesianPointList &vertexList) const
{
    ClusterVector clusterVector;
    LArPfoHelper::GetClusters(pPfo, hitType, clusterVector);

    for (ClusterVector::const_iterator iter = clusterVector.begin(), iterEnd = clusterVector.end(); iter != iterEnd; ++iter)
    {
        const Cluster *pCluster = *iter;

        CartesianVector firstCoordinate(0.f,0.f,0.f), secondCoordinate(0.f,0.f,0.f);
        LArClusterHelper::GetExtremalCoordinatesXZ(pCluster, firstCoordinate, secondCoordinate);

        vertexList.push_back(firstCoordinate);
        vertexList.push_back(secondCoordinate);
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

float DeltaRayIdentificationAlgorithm::GetTwoDSeparation(const CartesianPointList &vertexList, const ClusterVector &clusterVector) const
{
    if (vertexList.empty() || clusterVector.empty())
        throw StatusCodeException(STATUS_CODE_NOT_FOUND);

    float bestDisplacement(std::numeric_limits<float>::max());

    for (CartesianPointList::const_iterator iter1 = vertexList.begin(), iterEnd1 = vertexList.end(); iter1 != iterEnd1; ++iter1)
    {
        const CartesianVector &thisVertex = *iter1;

        for (ClusterVector::const_iterator iter2 = clusterVector.begin(), iterEnd2 = clusterVector.end(); iter2 != iterEnd2; ++iter2)
        {
            const Cluster *pCluster = *iter2;
            const float thisDisplacement(LArClusterHelper::GetClosestDistance(thisVertex, pCluster));

            if (thisDisplacement < bestDisplacement)
                bestDisplacement = thisDisplacement;
        }
    }

    return (bestDisplacement * bestDisplacement);
}

//------------------------------------------------------------------------------------------------------------------------------------------

void DeltaRayIdentificationAlgorithm::BuildParentDaughterLinks(const PfoAssociationMap &pfoAssociationMap, PfoList &daughterPfoList) const
{
    for (PfoAssociationMap::const_iterator iter = pfoAssociationMap.begin(), iterEnd = pfoAssociationMap.end(); iter != iterEnd; ++iter)
    {
        const ParticleFlowObject *pPfo = iter->first;

        ParticleFlowObject *pDaughterPfo = const_cast<ParticleFlowObject*>(pPfo);
        ParticleFlowObject *pParentPfo(this->GetParent(pfoAssociationMap, pDaughterPfo));

        if (NULL == pParentPfo)
            throw StatusCodeException(STATUS_CODE_FAILURE);

// --- BEGIN EVENT DISPLAY ---
// PfoList tempList1, tempList2;
// tempList1.insert(pParentPfo);
// tempList2.insert(pDaughterPfo);
// PandoraMonitoringApi::SetEveDisplayParameters(false, DETECTOR_VIEW_XZ);
// PandoraMonitoringApi::VisualizeParticleFlowObjects(&tempList1, "Parent", RED, false, false);
// PandoraMonitoringApi::VisualizeParticleFlowObjects(&tempList2, "Daughter", BLUE, false, false);
// PandoraMonitoringApi::ViewEvent();
// --- END EVENT DISPLAY ---

        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::SetPfoParentDaughterRelationship(*this, pParentPfo, pDaughterPfo));
        daughterPfoList.insert(pDaughterPfo);
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

ParticleFlowObject *DeltaRayIdentificationAlgorithm::GetParent(const PfoAssociationMap &pfoAssociationMap,
    const ParticleFlowObject *const pPfo) const
{
    ParticleFlowObject *pParentPfo = NULL;
    ParticleFlowObject *pDaughterPfo = const_cast<ParticleFlowObject*>(pPfo);

    while(1)
    {
        PfoAssociationMap::const_iterator iter = pfoAssociationMap.find(pDaughterPfo);
        if (pfoAssociationMap.end() == iter)
            break;

        pParentPfo = const_cast<ParticleFlowObject*>(iter->second);
        pDaughterPfo = pParentPfo;
    }

    return pParentPfo;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode DeltaRayIdentificationAlgorithm::ReadSettings(const TiXmlHandle xmlHandle)
{
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ReadValue(xmlHandle, "ParentPfoListName", m_parentPfoListName));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ReadValue(xmlHandle, "DaughterPfoListName", m_daughterPfoListName));

    m_distanceForMatching = 3.f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "DistanceForMatching", m_distanceForMatching));
    
    float minParentLength = 10.f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MinParentLength", minParentLength));
    m_minParentLengthSquared = minParentLength * minParentLength;

    float maxDaughterLength = 100.f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MaxDaughterLength", maxDaughterLength));
    m_maxDaughterLengthSquared = maxDaughterLength * maxDaughterLength;

    return STATUS_CODE_SUCCESS;
}

} // namespace lar
