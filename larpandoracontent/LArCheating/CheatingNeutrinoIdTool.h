/**
 *  @file   larpandoracontent/LArCheating/CheatingNeutrinoIdTool.h
 *
 *  @brief  Header file for the cheating neutrino id tool class.
 *
 *  $Log: $
 */
#ifndef LAR_CHEATING_NEUTRINO_ID_TOOL_H
#define LAR_CHEATING_NEUTRINO_ID_TOOL_H 1

#include "larpandoracontent/LArCheating/CheatingSliceIdBaseTool.h"
#include "larpandoracontent/LArControlFlow/MasterAlgorithm.h"

namespace lar_content
{

/**
 *  @brief  CheatingNeutrinoIdTool class
 */
class CheatingNeutrinoIdTool : public CheatingSliceIdBaseTool
{
public:
    void SelectOutputPfos(const pandora::Algorithm *const pAlgorithm, const SliceHypotheses &nuSliceHypotheses, const SliceHypotheses &crSliceHypotheses, pandora::PfoList &selectedPfos);

private:
    pandora::StatusCode ReadSettings(const pandora::TiXmlHandle xmlHandle);
    
    /**
     *  @brief  ...
     */
    void GetNeutrinoWeight(const pandora::CaloHit *const pCaloHit, float &neutrinoWeight, float &totalWeight) const;
    
    /**
     *  @brief  ...
     */
    void GetNeutrinoWeight(const pandora::PfoList *const pPfoList, float &neutrinoWeight, float &totalWeight) const;
};

} // namespace lar_content

#endif // #ifndef LAR_CHEATING_NEUTRINO_ID_TOOL_H
