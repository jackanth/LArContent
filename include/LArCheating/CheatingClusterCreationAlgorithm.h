/**
 *  @file   LArContent/include/LArCheating/CheatingClusterCreationAlgorithm.h
 * 
 *  @brief  Header file for the cheating cluster creation algorithm class.
 * 
 *  $Log: $
 */
#ifndef LAR_CHEATING_CLUSTER_CREATION_ALGORITHM_H
#define LAR_CHEATING_CLUSTER_CREATION_ALGORITHM_H 1

#include "Pandora/Algorithm.h"

#include <unordered_map>

namespace lar_content
{

/**
 *  @brief  CheatingClusterCreationAlgorithm class
 */
class CheatingClusterCreationAlgorithm : public pandora::Algorithm
{
public:
    /**
     *  @brief  Factory class for instantiating algorithm
     */
    class Factory : public pandora::AlgorithmFactory
    {
    public:
        pandora::Algorithm *CreateAlgorithm() const;
    };

private:
    pandora::StatusCode Run();
    pandora::StatusCode ReadSettings(const pandora::TiXmlHandle xmlHandle);

    typedef std::unordered_map<const pandora::MCParticle*, pandora::CaloHitList> MCParticleToHitListMap;

    /**
     *  @brief  Simple mc particle collection, using main mc particle associated with each calo hit
     * 
     *  @param  pCaloHit address of the calo hit
     *  @param  mcParticleToHitListMap the mc particle to hit list map
     */
    void SimpleMCParticleCollection(const pandora::CaloHit *const pCaloHit, MCParticleToHitListMap &mcParticleToHitListMap) const;

    /**
     *  @brief  Check whether mc particle is of a type specified for inclusion in cheated clustering
     * 
     *  @param  pMCParticle the mc particle to hit list map
     * 
     *  @return boolean
     */
    bool SelectMCParticlesForClustering(const pandora::MCParticle *const pMCParticle) const;

    /**
     *  @brief  Create clusters based on information in the mc particle to hit list map
     * 
     *  @param  mcParticleToHitListMap the mc particle to hit list map
     */
    void CreateClusters(const MCParticleToHitListMap &mcParticleToHitListMap) const;

    pandora::IntVector  m_particleIdList;               ///< list of particle ids of MCPFOs to be selected
};

//------------------------------------------------------------------------------------------------------------------------------------------

inline pandora::Algorithm *CheatingClusterCreationAlgorithm::Factory::CreateAlgorithm() const
{
    return new CheatingClusterCreationAlgorithm();
}

} // namespace lar_content

#endif // #ifndef LAR_CHEATING_CLUSTER_CREATION_ALGORITHM_H
