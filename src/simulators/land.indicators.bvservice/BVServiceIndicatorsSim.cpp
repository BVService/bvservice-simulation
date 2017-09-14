/**
  @file MySim.cpp
*/


/*
<sim2doc>

</sim2doc>
*/


#include <limits>

#include <openfluid/ware/PluggableSimulator.hpp>
#include <openfluid/tools/DataHelpers.hpp>
#include <openfluid/scientific/FloatingPoint.hpp>


// =====================================================================
// =====================================================================


BEGIN_SIMULATOR_SIGNATURE("land.indicators.bvservice")

  // Informations
  DECLARE_NAME("")
  DECLARE_DESCRIPTION("")
  DECLARE_VERSION("")
  DECLARE_STATUS(openfluid::ware::EXPERIMENTAL)

  DECLARE_REQUIRED_ATTRIBUTE("area","SU","","m")
  DECLARE_REQUIRED_ATTRIBUTE("slopemean","SU","","m")
  DECLARE_REQUIRED_ATTRIBUTE("length","LI","","m")
  DECLARE_REQUIRED_ATTRIBUTE("grassbsratio","LI","","m")
  DECLARE_REQUIRED_ATTRIBUTE("hedgesratio","LI","","m")

  DECLARE_REQUIRED_VARIABLE("runoffvolume","SU","","m3")
  DECLARE_REQUIRED_VARIABLE("uprunoffvolume","SU","","m3")
  DECLARE_REQUIRED_VARIABLE("runoffvolume","LI","outgoing runoff volume","m3")
  DECLARE_REQUIRED_VARIABLE("uprunoffvolume","LI","incoming runoff volume","m3")
  DECLARE_REQUIRED_VARIABLE("infiltvolume","LI","","m3")


  DECLARE_PRODUCED_VARIABLE("upperarea","SU","Contributive upper area","m")
  DECLARE_PRODUCED_VARIABLE("bufferscount[integer]","SU","number of buffer elements crossed to reach the hyrological network","")
  DECLARE_PRODUCED_VARIABLE("infiltvolsum","SU","","m")
  DECLARE_PRODUCED_VARIABLE("runoffvoldelta","SU","","m3")
  DECLARE_PRODUCED_VARIABLE("runoffvolratio","SU","","")
  DECLARE_PRODUCED_VARIABLE("infiltvolratio","SU","","")
  DECLARE_PRODUCED_VARIABLE("infiltvolratiosum","SU","","")
  DECLARE_PRODUCED_VARIABLE("conndegree","SU","","")
  DECLARE_PRODUCED_VARIABLE("erosionrisk","SU","","m3")
  DECLARE_PRODUCED_VARIABLE("runoffcontrib","SU","","m3")

  DECLARE_PRODUCED_VARIABLE("upperarea","LI","","m")
  DECLARE_PRODUCED_VARIABLE("runoffvoldelta","LI","","m3")
  DECLARE_PRODUCED_VARIABLE("runoffvolratio","LI","","")
  DECLARE_PRODUCED_VARIABLE("concdegree","LI","","")
  DECLARE_PRODUCED_VARIABLE("infiltvolratio","LI","","")
  DECLARE_PRODUCED_VARIABLE("importancedegree","LI","degree of importance","")
  DECLARE_PRODUCED_VARIABLE("interestdegree","LI","degree of potential interest","")

  DECLARE_PRODUCED_VARIABLE("upperarea","RS","","m")

END_SIMULATOR_SIGNATURE


// =====================================================================
// =====================================================================


/**

*/
class BVServiceIndicatorsSimulator : public openfluid::ware::PluggableSimulator
{
  private:

    typedef std::map<std::string,double> UpperAreaMap_t;


  public:


    BVServiceIndicatorsSimulator(): PluggableSimulator()
    {


    }


    // =====================================================================
    // =====================================================================


    ~BVServiceIndicatorsSimulator()
    {


    }


    // =====================================================================
    // =====================================================================


    void normalizeVariable(const openfluid::core::UnitsClass_t& ClassName,
                           const openfluid::core::VariableName_t& VarName)
    {
      openfluid::core::SpatialUnit* U;
      double Max = std::numeric_limits<double>::min();
      double Min = std::numeric_limits<double>::max();

      OPENFLUID_UNITS_ORDERED_LOOP(ClassName,U)
      {
        double Val = OPENFLUID_GetLatestVariable(U,VarName).value()->asDoubleValue().get();

        if (!std::isnan(Val))
        {
          Min = std::min(Min,Val);
          Max = std::max(Max,Val);
        }
      }

      // OPENFLUID_DisplayInfo("Variable " << VarName << " on " << ClassName << " : " << Min << " -> " << Max);

      OPENFLUID_UNITS_ORDERED_LOOP(ClassName,U)
      {
        double Val = OPENFLUID_GetLatestVariable(U,VarName).value()->asDoubleValue().get();

        if (!std::isnan(Val))
        {
          double NormVal = (Val - Min) / (Max - Min);
          OPENFLUID_SetVariable(U,VarName,NormVal);
        }
      }
    }


    // =====================================================================
    // =====================================================================

    std::string getXClassID(const openfluid::core::SpatialUnit* U)
    {
      return U->getClass()+"#"+openfluid::tools::convertValue(U->getID());
    }


    // =====================================================================
    // =====================================================================


    void computeUpperUpperAreaRecursively(openfluid::core::SpatialUnit* U,
                                          UpperAreaMap_t& MapArea)
    {

      // postfixed depth-first search to process leafs first and go back to root


      std::vector<openfluid::core::SpatialUnit*> UpperUnits;


      // --- go to leafs recursively
      openfluid::core::UnitsPtrList_t* UpperLI = U->fromSpatialUnits("LI");
      openfluid::core::UnitsPtrList_t* UpperSU = U->fromSpatialUnits("SU");



      if (UpperLI)
      {
        openfluid::core::SpatialUnit* UpU;

        OPENFLUID_UNITSLIST_LOOP(UpperLI,UpU)
        {
          UpperUnits.push_back(UpU);
        }
      }


      if (UpperSU)
      {
        openfluid::core::SpatialUnit* UpU;

        OPENFLUID_UNITSLIST_LOOP(UpperSU,UpU)
        {
          UpperUnits.push_back(UpU);
        }
      }



      for (auto UpU : UpperUnits)
      {
        computeUpperUpperAreaRecursively(UpU,MapArea);
      }



      double UpperAreaSum = 0.0;
      double Area = 0;

      for (auto UpU : UpperUnits)
      {
        UpperAreaSum = UpperAreaSum + MapArea[getXClassID(UpU)];
      }

      if (U->getClass() == "SU")
        OPENFLUID_GetAttribute(U,"area",Area);

      UpperAreaSum = UpperAreaSum + Area;
      MapArea[getXClassID(U)] = UpperAreaSum;

    }


    // =====================================================================
    // =====================================================================


    void computeBuffersCountToNetworkRecursively(openfluid::core::SpatialUnit* U, int Counter)
    {

      // prefixed depth-first search to process from root to leafs

//      OPENFLUID_DisplayInfo("BuffersCount : entering unit " << U->getClass() << "#" << U->getID());

      if (U->getClass() == "SU")
        OPENFLUID_AppendVariable(U,"bufferscount",(long)Counter);

      int NewCounter = Counter;

      if (U->getClass() == "LI")
      {
        double HedgeRatio, GrassRatio, BenchRatio;
        OPENFLUID_GetAttribute(U,"hedgesratio",HedgeRatio);
        OPENFLUID_GetAttribute(U,"grassbsratio",GrassRatio);
        OPENFLUID_GetAttribute(U,"benchesratio",BenchRatio);

        if (GrassRatio+HedgeRatio+BenchRatio > 0)
          NewCounter++;
      }
      else if (U->getClass() == "SU")
      {
        std::string LandUse;
        OPENFLUID_GetAttribute(U,"landuse",LandUse);
        if (LandUse == "buffer") // TODO uncorrect to fix
          NewCounter++;
      }


      std::set<openfluid::core::SpatialUnit*> UpperUnits;

      openfluid::core::UnitsPtrList_t* TmpUpperList;
      TmpUpperList = U->fromSpatialUnits("SU");
      if (TmpUpperList != nullptr)
      {
        for (auto TmpU : *TmpUpperList)
          UpperUnits.insert(TmpU);
      }


      TmpUpperList = U->fromSpatialUnits("LI");
      if (TmpUpperList != nullptr)
      {
        for (auto TmpU : *TmpUpperList)
          UpperUnits.insert(TmpU);
      }


      if (!UpperUnits.empty()) //if UpperUnits not empty, upstream Units are present
      {
        for(auto UpU : UpperUnits) // loop for each upstream unit
        {
          computeBuffersCountToNetworkRecursively(UpU,NewCounter);
        }
      }

    }


    // =====================================================================
    // =====================================================================


    void computeCumulatedInfiltrationFromNetworkRecursively(openfluid::core::SpatialUnit* U, double InfiltSum)
    {

      // prefixed depth-first search to process from root to leafs

      // OPENFLUID_DisplayInfo("Infilt sum : entering unit " << U->getClass() << "#" << U->getID());

      double  NewInfiltSum = InfiltSum;

      if (U->getClass() == "SU")
      {
        NewInfiltSum = NewInfiltSum + OPENFLUID_GetVariable(U,"infiltvolume")->asDoubleValue();
        OPENFLUID_AppendVariable(U,"infiltvolsum",NewInfiltSum);
      }



      std::set<openfluid::core::SpatialUnit*> UpperUnits;

      openfluid::core::UnitsPtrList_t* TmpUpperList;
      TmpUpperList = U->fromSpatialUnits("SU");
      if (TmpUpperList != nullptr)

      {
        for (auto TmpU : *TmpUpperList)
          UpperUnits.insert(TmpU);
      }


      TmpUpperList = U->fromSpatialUnits("LI");
      if (TmpUpperList != nullptr)
      {
        for (auto TmpU : *TmpUpperList)
          UpperUnits.insert(TmpU);
      }



      if (!UpperUnits.empty()) //if UpperUnits not empty, upstream Units are present
      {
        for(auto UpU : UpperUnits) // loop for each upstream unit
        {
          computeCumulatedInfiltrationFromNetworkRecursively(UpU,NewInfiltSum);
        }
      }

    }


    // =====================================================================
    // =====================================================================


    void computeCumulatedInfilRatioFromNetworkRecursively(openfluid::core::SpatialUnit* U, double InfiltSum)
    {

      // prefixed depth-first search to process from root to leafs

      // OPENFLUID_DisplayInfo("Infilt ratio sum : entering unit " << U->getClass() << "#" << U->getID());

      double  NewInfiltSum = InfiltSum;

      if (U->getClass() == "SU" || U->getClass() == "LI")
      {
        NewInfiltSum = NewInfiltSum + OPENFLUID_GetVariable(U,"infiltvolratio")->asDoubleValue();
      }

      if (U->getClass() == "SU")
        OPENFLUID_AppendVariable(U,"infiltvolratiosum",NewInfiltSum);

      std::set<openfluid::core::SpatialUnit*> UpperUnits;

      openfluid::core::UnitsPtrList_t* TmpUpperList;
      TmpUpperList = U->fromSpatialUnits("SU");
      if (TmpUpperList != nullptr)

      {
        for (auto TmpU : *TmpUpperList)
          UpperUnits.insert(TmpU);
      }


      TmpUpperList = U->fromSpatialUnits("LI");
      if (TmpUpperList != nullptr)
      {
        for (auto TmpU : *TmpUpperList)
          UpperUnits.insert(TmpU);
      }


      if (!UpperUnits.empty()) //if UpperUnits not empty, upstream Units are present
      {
        for(auto UpU : UpperUnits) // loop for each upstream unit
        {

          computeCumulatedInfilRatioFromNetworkRecursively(UpU,NewInfiltSum);
        }
      }

    }


    // =====================================================================
    // =====================================================================


    void initParams(const openfluid::ware::WareParams_t& /*Params*/)
    {


    }


    // =====================================================================
    // =====================================================================


    void prepareData()
    {


    }


    // =====================================================================
    // =====================================================================


    void checkConsistency()
    {


    }


    // =====================================================================
    // =====================================================================


    openfluid::base::SchedulingRequest initializeRun()
    {
      openfluid::core::SpatialUnit* U;

      OPENFLUID_ALLUNITS_ORDERED_LOOP(U)
      {
        OPENFLUID_InitializeVariable(U,"upperarea",0.0);
      }

      OPENFLUID_UNITS_ORDERED_LOOP("SU",U)
      {
        OPENFLUID_InitializeVariable(U,"bufferscount",(long)0);
        OPENFLUID_InitializeVariable(U,"infiltvolsum",0.0);
        OPENFLUID_InitializeVariable(U,"infiltvolratiosum",0.0);

        OPENFLUID_InitializeVariable(U,"runoffvoldelta",0.0);
        OPENFLUID_InitializeVariable(U,"runoffvolratio",0.0);
        OPENFLUID_InitializeVariable(U,"infiltvolratio",0.0);
        OPENFLUID_InitializeVariable(U,"conndegree",0.0);
        OPENFLUID_InitializeVariable(U,"erosionrisk",0.0);
        OPENFLUID_InitializeVariable(U,"runoffcontrib",0.0);
      }

      OPENFLUID_UNITS_ORDERED_LOOP("LI",U)
      {
        OPENFLUID_InitializeVariable(U,"runoffvoldelta",0.0);
        OPENFLUID_InitializeVariable(U,"runoffvolratio",0.0);
        OPENFLUID_InitializeVariable(U,"concdegree",0.0);
        OPENFLUID_InitializeVariable(U,"importancedegree",0.0);
        OPENFLUID_InitializeVariable(U,"interestdegree",0.0);
        OPENFLUID_InitializeVariable(U,"infiltvolratio",0.0);
      }

      return DefaultDeltaT();
    }


    // =====================================================================
    // =====================================================================


    openfluid::base::SchedulingRequest runStep()
    {
      openfluid::core::SpatialUnit* U;


      // ============= Upper area


      UpperAreaMap_t UpperAreaMap;

      OPENFLUID_ALLUNITS_ORDERED_LOOP(U)
      {
        UpperAreaMap[getXClassID(U)] = 0.0;
      }

      OPENFLUID_ALLUNITS_ORDERED_LOOP(U)
      {
        if (U->toSpatialUnits("SU") == nullptr &&
            U->toSpatialUnits("RS") == nullptr &&
            U->toSpatialUnits("LI") == nullptr)
        {
          computeUpperUpperAreaRecursively(U,UpperAreaMap);
        }
      }


      OPENFLUID_ALLUNITS_ORDERED_LOOP(U)
      {
        OPENFLUID_AppendVariable(U,"upperarea",UpperAreaMap[getXClassID(U)]);
      }


      // ============= Buffers

      OPENFLUID_UNITS_ORDERED_LOOP("RS",U)
      {
        std::set<openfluid::core::SpatialUnit*> UpperUnits;

        openfluid::core::UnitsPtrList_t* TmpUpperList;
        TmpUpperList = U->fromSpatialUnits("SU");
        if (TmpUpperList != nullptr)
        {
          for (auto TmpU : *TmpUpperList)
            UpperUnits.insert(TmpU);
        }


        TmpUpperList = U->fromSpatialUnits("LI");
        if (TmpUpperList != nullptr)
        {
          for (auto TmpU : *TmpUpperList)
            UpperUnits.insert(TmpU);
        }


        if (!UpperUnits.empty()) //if UpperUnits not empty, upstream Units are present
        {
          for(auto UpU : UpperUnits) // loop for each upstream unit
          {
            computeBuffersCountToNetworkRecursively(UpU,0);
          }
        }

      }


      // ============= Cumulated infiltration to network


      OPENFLUID_UNITS_ORDERED_LOOP("RS",U)
      {
        std::set<openfluid::core::SpatialUnit*> UpperUnits;

        openfluid::core::UnitsPtrList_t* TmpUpperList;
        TmpUpperList = U->fromSpatialUnits("SU");
        if (TmpUpperList != nullptr)
        {
          for (auto TmpU : *TmpUpperList)
            UpperUnits.insert(TmpU);
        }


        TmpUpperList = U->fromSpatialUnits("LI");
        if (TmpUpperList != nullptr)
        {
          for (auto TmpU : *TmpUpperList)
            UpperUnits.insert(TmpU);
        }


        if (!UpperUnits.empty()) //if UpperUnits not empty, upstream Units are present
        {
          for(auto UpU : UpperUnits) // loop for each upstream unit
          {
            computeCumulatedInfiltrationFromNetworkRecursively(UpU,0.0);
          }
        }

      }


      // ============= SU delta volume and ratio volume


      OPENFLUID_UNITS_ORDERED_LOOP("SU",U)
      {
        double UpRunoffVol = OPENFLUID_GetVariable(U,"uprunoffvolume")->asDoubleValue().get();
        double RunoffVol = OPENFLUID_GetVariable(U,"runoffvolume")->asDoubleValue().get();
        double Slope = OPENFLUID_GetAttribute(U,"slopemean")->asDoubleValue().get();

        double DeltaVolume = RunoffVol - UpRunoffVol;

        double RatioVolume = 0.0;
        if (openfluid::scientific::isVeryClose(UpRunoffVol,0.0))
          RatioVolume = std::numeric_limits<double>::quiet_NaN();
        else if (openfluid::scientific::isVeryClose(DeltaVolume,0.0))
          RatioVolume = 0.0;
        else
          RatioVolume = DeltaVolume / UpRunoffVol;

        OPENFLUID_AppendVariable(U,"runoffvoldelta",DeltaVolume);
        OPENFLUID_AppendVariable(U,"runoffcontrib",DeltaVolume);
        OPENFLUID_AppendVariable(U,"runoffvolratio",RatioVolume);
        OPENFLUID_AppendVariable(U,"erosionrisk",RunoffVol*Slope);
      }


      // ============= LI delta volume, ratio volume and concentration degree


      OPENFLUID_UNITS_ORDERED_LOOP("LI",U)
      {
        double UpRunoffVol = OPENFLUID_GetVariable(U,"uprunoffvolume")->asDoubleValue().get();
        double RunoffVol = OPENFLUID_GetVariable(U,"runoffvolume")->asDoubleValue().get();

        double DeltaVolume = RunoffVol - UpRunoffVol;

        double RatioVolume = 0.0;
        if (openfluid::scientific::isVeryClose(UpRunoffVol,0.0))
          RatioVolume = std::numeric_limits<double>::quiet_NaN();
        else if (openfluid::scientific::isVeryClose(DeltaVolume,0.0))
          RatioVolume = 0.0;
        else
          RatioVolume = DeltaVolume / UpRunoffVol;

        OPENFLUID_AppendVariable(U,"runoffvoldelta",DeltaVolume);
        OPENFLUID_AppendVariable(U,"runoffvolratio",RatioVolume);

        double Length = OPENFLUID_GetAttribute(U,"length")->asDoubleValue().get();
        OPENFLUID_AppendVariable(U,"concdegree",UpRunoffVol/Length);
      }


      // ============= LI downstream infiltrated volume and importance degree

      static const std::list<std::string> Subparts = {"benches","grassbs","hedges"};

      OPENFLUID_UNITS_ORDERED_LOOP("LI",U)
      {
        // Former method
        /*openfluid::core::UnitsPtrList_t* TmpDownList;

        TmpDownList = U->toSpatialUnits("SU");


        double SUNegRatioSum = 0.0;
        double LIRatioSum  = 0.0;

        if (TmpDownList)
        {
          for (auto TmpU : *TmpDownList)
          {
            double RunoffVol = OPENFLUID_GetVariable(TmpU,"runoffvolratio")->asDoubleValue().get();
            if (RunoffVol < 0.0)
              SUNegRatioSum += RunoffVol;
          }
        }

        TmpDownList = U->toSpatialUnits("LI");
        if (TmpDownList)
        {
          for (auto TmpU : *TmpDownList)
          {
            double RunoffVol = OPENFLUID_GetVariable(TmpU,"runoffvolratio")->asDoubleValue().get();
            LIRatioSum += RunoffVol;
          }
        }

        double InfiltVolRatioDown = SUNegRatioSum+LIRatioSum;
        OPENFLUID_AppendVariable(U,"infiltvolratio",SUNegRatioSum+LIRatioSum);

        double UpRunoffVol = OPENFLUID_GetVariable(U,"uprunoffvolume")->asDoubleValue().get();
        double RunoffVol = OPENFLUID_GetVariable(U,"runoffvolume")->asDoubleValue().get();
        OPENFLUID_AppendVariable(U,"importancedegree",UpRunoffVol-(RunoffVol*InfiltVolRatioDown));
        */

        double InfiltVol = OPENFLUID_GetVariable(U,"infiltvolume")->asDoubleValue().get();
        double UpRunoffVol = OPENFLUID_GetVariable(U,"uprunoffvolume")->asDoubleValue().get();

        if (openfluid::scientific::isVeryClose(UpRunoffVol,0.0))
          OPENFLUID_AppendVariable(U,"infiltvolratio",0.0);
        else
          OPENFLUID_AppendVariable(U,"infiltvolratio",InfiltVol/UpRunoffVol);



        bool Occupied = false;

        for (auto LinearPart : Subparts)
        {
          double Ratio = 0.0;
          OPENFLUID_GetAttribute(U,LinearPart+"ratio",Ratio);

          if (Ratio > 0.0)
            Occupied = true;
        }

        if (Occupied)
        {
          OPENFLUID_AppendVariable(U,"importancedegree",InfiltVol);
          OPENFLUID_AppendVariable(U,"interestdegree",std::numeric_limits<double>::quiet_NaN());
        }
        else
        {
          OPENFLUID_AppendVariable(U,"importancedegree",std::numeric_limits<double>::quiet_NaN());
          OPENFLUID_AppendVariable(U,"interestdegree",UpRunoffVol);
        }
      }


      // ============= SU infiltration ratio

      OPENFLUID_UNITS_ORDERED_LOOP("SU",U)
      {
        openfluid::core::UnitsPtrList_t* TmpUpList;

        TmpUpList = U->fromSpatialUnits("SU");


        double UpRunoffVol = 0.0;

        if (TmpUpList)
        {
          for (auto TmpU : *TmpUpList)
          {
            double RunoffVol = OPENFLUID_GetVariable(TmpU,"runoffvolume")->asDoubleValue().get();
            UpRunoffVol += RunoffVol;
          }
        }


        TmpUpList = U->fromSpatialUnits("LI");
        if (TmpUpList)
        {
          for (auto TmpU : *TmpUpList)
          {
            double RunoffVol = OPENFLUID_GetVariable(TmpU,"runoffvolume")->asDoubleValue().get();
            UpRunoffVol += RunoffVol;
          }
        }



        double InfiltVol = OPENFLUID_GetVariable(U,"infiltvolume")->asDoubleValue().get();

        if (openfluid::scientific::isVeryClose(UpRunoffVol,0.0))
          OPENFLUID_AppendVariable(U,"infiltvolratio",0.0);
        else
          OPENFLUID_AppendVariable(U,"infiltvolratio",InfiltVol/UpRunoffVol);

      }


      // ============= SU downstream infiltrated volume and connectivity degree

      OPENFLUID_UNITS_ORDERED_LOOP("RS",U)
      {
        computeCumulatedInfilRatioFromNetworkRecursively(U,0);
      }

      OPENFLUID_UNITS_ORDERED_LOOP("SU",U)
      {
        double InfiltVolRatioSum;
        InfiltVolRatioSum = OPENFLUID_GetLatestVariable(U,"infiltvolratiosum").value()->asDoubleValue().get();
        OPENFLUID_AppendVariable(U,"conndegree",InfiltVolRatioSum);
      }


      normalizeVariable("SU","conndegree");
      normalizeVariable("SU","erosionrisk");
      normalizeVariable("SU","runoffcontrib");

      normalizeVariable("LI","importancedegree");
      normalizeVariable("LI","interestdegree");
      normalizeVariable("LI","concdegree");

      return DefaultDeltaT();
    }


    // =====================================================================
    // =====================================================================


    void finalizeRun()
    {


    }

};


// =====================================================================
// =====================================================================


DEFINE_SIMULATOR_CLASS(BVServiceIndicatorsSimulator);


DEFINE_WARE_LINKUID(WARE_LINKUID)
