/**
 *  @file   larpandoracontent/LArThreeDReco/LArThreeDBase/ThreeDTracksBaseAlgorithm.cc
 * 
 *  @brief  Implementation of the three dimensional tracks tracks algorithm base class.
 * 
 *  $Log: $
 */

#include "Pandora/AlgorithmHeaders.h"

#include "larpandoracontent/LArHelpers/LArClusterHelper.h"
#include "larpandoracontent/LArHelpers/LArGeometryHelper.h"

#include "larpandoracontent/LArObjects/LArPointingCluster.h"
#include "larpandoracontent/LArObjects/LArTrackOverlapResult.h"

#include "larpandoracontent/LArThreeDReco/LArThreeDBase/ThreeDTracksBaseAlgorithm.h"

using namespace pandora;

namespace lar_content
{

template<typename T>
ThreeDTracksBaseAlgorithm<T>::ThreeDTracksBaseAlgorithm() :
    m_slidingFitWindow(20),
    m_minClusterCaloHits(5),
    m_minClusterLengthSquared(3.f * 3.f)
{
}

//------------------------------------------------------------------------------------------------------------------------------------------

template <typename T>
ThreeDTracksBaseAlgorithm<T>::~ThreeDTracksBaseAlgorithm()
{
}

//------------------------------------------------------------------------------------------------------------------------------------------

template<typename T>
const TwoDSlidingFitResult &ThreeDTracksBaseAlgorithm<T>::GetCachedSlidingFitResult(const Cluster *const pCluster) const
{
    TwoDSlidingFitResultMap::const_iterator iter = m_slidingFitResultMap.find(pCluster);

    if (m_slidingFitResultMap.end() == iter)
        throw StatusCodeException(STATUS_CODE_NOT_FOUND);

    return iter->second;
}

//------------------------------------------------------------------------------------------------------------------------------------------

template <typename T>
bool ThreeDTracksBaseAlgorithm<T>::MakeClusterSplits(const SplitPositionMap &splitPositionMap)
{
    bool changesMade(false);

    ClusterList splitClusters;
    for (const auto &mapEntry : splitPositionMap) splitClusters.push_back(mapEntry.first);
    splitClusters.sort(LArClusterHelper::SortByNHits);

    for (const Cluster *pCurrentCluster : splitClusters)
    {
        CartesianPointVector splitPositions(splitPositionMap.at(pCurrentCluster));
        std::sort(splitPositions.begin(), splitPositions.end(), ThreeDTracksBaseAlgorithm::SortSplitPositions);

        const HitType hitType(LArClusterHelper::GetClusterHitType(pCurrentCluster));
        const std::string clusterListName((TPC_VIEW_U == hitType) ? this->GetClusterListNameU() : (TPC_VIEW_V == hitType) ? this->GetClusterListNameV() : this->GetClusterListNameW());

        if (!((TPC_VIEW_U == hitType) || (TPC_VIEW_V == hitType) || (TPC_VIEW_W == hitType)))
            throw StatusCodeException(STATUS_CODE_FAILURE);

        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::ReplaceCurrentList<Cluster>(*this, clusterListName));

        for (CartesianPointVector::const_iterator sIter = splitPositions.begin(), sIterEnd = splitPositions.end(); sIter != sIterEnd; ++sIter)
        {
            const Cluster *pLowXCluster(NULL), *pHighXCluster(NULL);
            this->UpdateUponDeletion(pCurrentCluster);

            if (this->MakeClusterSplit(*sIter, pCurrentCluster, pLowXCluster, pHighXCluster))
            {
                changesMade = true;
                this->UpdateForNewCluster(pLowXCluster);
                this->UpdateForNewCluster(pHighXCluster);
                pCurrentCluster = pHighXCluster;
            }
            else
            {
                this->UpdateForNewCluster(pCurrentCluster);
            }
        }
    }

    return changesMade;
}

//------------------------------------------------------------------------------------------------------------------------------------------

template <typename T>
bool ThreeDTracksBaseAlgorithm<T>::MakeClusterSplit(const CartesianVector &splitPosition, const Cluster *&pCurrentCluster, const Cluster *&pLowXCluster,
    const Cluster *&pHighXCluster) const
{
    CartesianVector lowXEnd(0.f, 0.f, 0.f), highXEnd(0.f, 0.f, 0.f);

    try
    {
        LArPointingCluster pointingCluster(pCurrentCluster);
        const bool innerIsLowX(pointingCluster.GetInnerVertex().GetPosition().GetX() < pointingCluster.GetOuterVertex().GetPosition().GetX());
        lowXEnd = (innerIsLowX ? pointingCluster.GetInnerVertex().GetPosition() : pointingCluster.GetOuterVertex().GetPosition());
        highXEnd = (innerIsLowX ? pointingCluster.GetOuterVertex().GetPosition() : pointingCluster.GetInnerVertex().GetPosition());
    }
    catch (const StatusCodeException &) {return false;}

    const CartesianVector lowXUnitVector((lowXEnd - splitPosition).GetUnitVector());
    const CartesianVector highXUnitVector((highXEnd - splitPosition).GetUnitVector());

    CaloHitList caloHitList;
    pCurrentCluster->GetOrderedCaloHitList().FillCaloHitList(caloHitList);

    std::string originalListName, fragmentListName;
    const ClusterList clusterList(1, pCurrentCluster);
    PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::InitializeFragmentation(*this, clusterList, originalListName, fragmentListName));

    pLowXCluster = NULL;
    pHighXCluster = NULL;

    for (CaloHitList::const_iterator hIter = caloHitList.begin(), hIterEnd = caloHitList.end(); hIter != hIterEnd; ++hIter)
    {
        const CaloHit *const pCaloHit = *hIter;
        const CartesianVector unitVector((pCaloHit->GetPositionVector() - splitPosition).GetUnitVector());

        const float dotProductLowX(unitVector.GetDotProduct(lowXUnitVector));
        const float dotProductHighX(unitVector.GetDotProduct(highXUnitVector));
        const Cluster *&pClusterToModify((dotProductLowX > dotProductHighX) ? pLowXCluster : pHighXCluster);

        if (NULL == pClusterToModify)
        {
            PandoraContentApi::Cluster::Parameters parameters;
            parameters.m_caloHitList.push_back(pCaloHit);
            PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::Cluster::Create(*this, parameters, pClusterToModify));
        }
        else
        {
            PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::AddToCluster(*this, pClusterToModify, pCaloHit));
        }
    }

    if ((NULL == pLowXCluster) || (NULL == pHighXCluster))
    {
        PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::EndFragmentation(*this, originalListName, fragmentListName));
        return false;
    }

    PANDORA_THROW_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::EndFragmentation(*this, fragmentListName, originalListName));
    return true;
}

//------------------------------------------------------------------------------------------------------------------------------------------

template <typename T>
bool ThreeDTracksBaseAlgorithm<T>::SortSplitPositions(const pandora::CartesianVector &lhs, const pandora::CartesianVector &rhs)
{
    return (lhs.GetX() < rhs.GetX());
}

//------------------------------------------------------------------------------------------------------------------------------------------

template<typename T>
void ThreeDTracksBaseAlgorithm<T>::UpdateForNewCluster(const Cluster *const pNewCluster)
{
    try
    {
        this->AddToSlidingFitCache(pNewCluster);
    }
    catch (StatusCodeException &statusCodeException)
    {
        if (STATUS_CODE_FAILURE == statusCodeException.GetStatusCode())
            throw statusCodeException;

        return;
    }

    ThreeDBaseAlgorithm<T>::UpdateForNewCluster(pNewCluster);
}

//------------------------------------------------------------------------------------------------------------------------------------------

template<typename T>
void ThreeDTracksBaseAlgorithm<T>::UpdateUponDeletion(const Cluster *const pDeletedCluster)
{
    this->RemoveFromSlidingFitCache(pDeletedCluster);
    ThreeDBaseAlgorithm<T>::UpdateUponDeletion(pDeletedCluster);
}

//------------------------------------------------------------------------------------------------------------------------------------------

template <typename T>
void ThreeDTracksBaseAlgorithm<T>::SelectInputClusters(const ClusterList *const pInputClusterList, ClusterList &selectedClusterList) const
{
    for (ClusterList::const_iterator iter = pInputClusterList->begin(), iterEnd = pInputClusterList->end(); iter != iterEnd; ++iter)
    {
        const Cluster *const pCluster = *iter;

        if (!pCluster->IsAvailable())
            continue;

        if (pCluster->GetNCaloHits() < m_minClusterCaloHits)
            continue;

        if (LArClusterHelper::GetLengthSquared(pCluster) < m_minClusterLengthSquared)
            continue;

        selectedClusterList.push_back(pCluster);
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

template<typename T>
void ThreeDTracksBaseAlgorithm<T>::SetPfoParameters(const ProtoParticle &protoParticle, PandoraContentApi::ParticleFlowObject::Parameters &pfoParameters) const
{
    // TODO Correct these placeholder parameters
    pfoParameters.m_particleId = MU_MINUS; // Track
    pfoParameters.m_charge = PdgTable::GetParticleCharge(pfoParameters.m_particleId.Get());
    pfoParameters.m_mass = PdgTable::GetParticleMass(pfoParameters.m_particleId.Get());
    pfoParameters.m_energy = 0.f;
    pfoParameters.m_momentum = CartesianVector(0.f, 0.f, 0.f);
    pfoParameters.m_clusterList.insert(pfoParameters.m_clusterList.end(), protoParticle.m_clusterListU.begin(), protoParticle.m_clusterListU.end());
    pfoParameters.m_clusterList.insert(pfoParameters.m_clusterList.end(), protoParticle.m_clusterListV.begin(), protoParticle.m_clusterListV.end());
    pfoParameters.m_clusterList.insert(pfoParameters.m_clusterList.end(), protoParticle.m_clusterListW.begin(), protoParticle.m_clusterListW.end());
}

//------------------------------------------------------------------------------------------------------------------------------------------

template<typename T>
void ThreeDTracksBaseAlgorithm<T>::PreparationStep()
{
    this->PreparationStep(this->m_clusterListU);
    this->PreparationStep(this->m_clusterListV);
    this->PreparationStep(this->m_clusterListW);
}

//------------------------------------------------------------------------------------------------------------------------------------------

template<typename T>
void ThreeDTracksBaseAlgorithm<T>::PreparationStep(ClusterList &clusterList)
{
    for (ClusterList::iterator iter = clusterList.begin(), iterEnd = clusterList.end(); iter != iterEnd; )
    {
        const Cluster *const pCluster(*iter);

        try
        {
            this->AddToSlidingFitCache(pCluster);
            ++iter;
        }
        catch (StatusCodeException &statusCodeException)
        {
            clusterList.erase(iter++);

            if (STATUS_CODE_FAILURE == statusCodeException.GetStatusCode())
                throw statusCodeException;
        }
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

template<typename T>
void ThreeDTracksBaseAlgorithm<T>::TidyUp()
{
    m_slidingFitResultMap.clear();
    return ThreeDBaseAlgorithm<T>::TidyUp();
}

//------------------------------------------------------------------------------------------------------------------------------------------

template<typename T>
void ThreeDTracksBaseAlgorithm<T>::AddToSlidingFitCache(const Cluster *const pCluster)
{
    const float slidingFitPitch(LArGeometryHelper::GetWireZPitch(this->GetPandora()));
    const TwoDSlidingFitResult slidingFitResult(pCluster, m_slidingFitWindow, slidingFitPitch);

    if (!m_slidingFitResultMap.insert(TwoDSlidingFitResultMap::value_type(pCluster, slidingFitResult)).second)
        throw StatusCodeException(STATUS_CODE_FAILURE);
}

//------------------------------------------------------------------------------------------------------------------------------------------

template<typename T>
void ThreeDTracksBaseAlgorithm<T>::RemoveFromSlidingFitCache(const Cluster *const pCluster)
{
    TwoDSlidingFitResultMap::iterator iter = m_slidingFitResultMap.find(pCluster);

    if (m_slidingFitResultMap.end() != iter)
        m_slidingFitResultMap.erase(iter);
}

//------------------------------------------------------------------------------------------------------------------------------------------

template<typename T>
StatusCode ThreeDTracksBaseAlgorithm<T>::ReadSettings(const TiXmlHandle xmlHandle)
{
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "SlidingFitWindow", m_slidingFitWindow));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MinClusterCaloHits", m_minClusterCaloHits));

    float minClusterLength = std::sqrt(m_minClusterLengthSquared);
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MinClusterLength", minClusterLength));
    m_minClusterLengthSquared = minClusterLength * minClusterLength;

    return ThreeDBaseAlgorithm<T>::ReadSettings(xmlHandle);
}

template class ThreeDTracksBaseAlgorithm<TransverseOverlapResult>;
template class ThreeDTracksBaseAlgorithm<LongitudinalOverlapResult>;
template class ThreeDTracksBaseAlgorithm<FragmentOverlapResult>;

} // namespace lar_content
