// -*- C++ -*-
//
// Package:    BTagAnalyzerLiteT
// Class:      BTagAnalyzerLiteT
//
/**\class BTagAnalyzerLiteT BTagAnalyzerLite.cc RecoBTag/BTagAnalyzerLite/plugins/BTagAnalyzerLite.cc

Description: <one line class summary>

Implementation:
<Notes on implementation>
*/
//
// Original Author:  Andrea Jeremy
//         Created:  Thu Dec 20 10:00:00 CEST 2012
//
//
//

// system include files
#include <memory>

// user include files
#include "FWCore/Common/interface/Provenance.h"
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/TriggerNamesService.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "FWCore/Utilities/interface/RegexMatch.h"

#include "DataFormats/Candidate/interface/VertexCompositePtrCandidate.h"
#include "DataFormats/Common/interface/TriggerResults.h"
#include "DataFormats/BTauReco/interface/JetTag.h"
#include "DataFormats/BTauReco/interface/CandIPTagInfo.h"
#include "DataFormats/BTauReco/interface/TrackIPTagInfo.h"
#include "DataFormats/BTauReco/interface/CandSoftLeptonTagInfo.h"
#include "DataFormats/GeometrySurface/interface/Line.h"
#include "DataFormats/GeometryCommonDetAlgo/interface/Measurement1D.h"
#include "DataFormats/HepMCCandidate/interface/GenParticleFwd.h"
#include "DataFormats/HepMCCandidate/interface/GenParticle.h"
#include "DataFormats/Math/interface/deltaR.h"
#include "DataFormats/JetReco/interface/GenJetCollection.h"
#include "DataFormats/ParticleFlowCandidate/interface/PFCandidate.h"
#include "DataFormats/PatCandidates/interface/Jet.h"
#include "DataFormats/PatCandidates/interface/Muon.h"
#include "DataFormats/PatCandidates/interface/PackedCandidate.h"
#include "DataFormats/TrackReco/interface/TrackFwd.h"
#include "DataFormats/TrackReco/interface/Track.h"
#include "DataFormats/VertexReco/interface/Vertex.h"

#include "SimDataFormats/GeneratorProducts/interface/GenEventInfoProduct.h"
#include "SimDataFormats/PileupSummaryInfo/interface/PileupSummaryInfo.h"

#include "CommonTools/UtilAlgos/interface/TFileService.h"

#include "PhysicsTools/SelectorUtils/interface/PFJetIDSelectionFunctor.h"

#include "RecoBTau/JetTagComputer/interface/GenericMVAJetTagComputer.h"
#include "RecoBTau/JetTagComputer/interface/GenericMVAJetTagComputerWrapper.h"
#include "RecoBTau/JetTagComputer/interface/JetTagComputer.h"
#include "RecoBTau/JetTagComputer/interface/JetTagComputerRecord.h"
#include "RecoBTag/SecondaryVertex/interface/TrackKinematics.h"
#include "RecoVertex/VertexPrimitives/interface/ConvertToFromReco.h"

#include "fastjet/contrib/Njettiness.hh"

#include "TFile.h"
#include "TTree.h"

#include <boost/regex.hpp>

#include "RecoBTag/BTagAnalyzerLite/interface/JetInfoBranches.h"
#include "RecoBTag/BTagAnalyzerLite/interface/EventInfoBranches.h"

//
// constants, enums and typedefs
//
typedef std::vector<pat::Jet> PatJetCollection;


//
// class declaration
//

struct orderByPt {
    const std::string mCorrLevel;
    orderByPt(const std::string& fCorrLevel) : mCorrLevel(fCorrLevel) {}
    bool operator ()(PatJetCollection::const_iterator const& a, PatJetCollection::const_iterator const& b) {
      if( mCorrLevel=="Uncorrected" )
        return a->correctedJet("Uncorrected").pt() > b->correctedJet("Uncorrected").pt();
      else
        return a->pt() > b->pt();
    }
};

const math::XYZPoint & position(const reco::Vertex & sv) {return sv.position();}
const math::XYZPoint & position(const reco::VertexCompositePtrCandidate & sv) {return sv.vertex();}
const double xError(const reco::Vertex & sv) {return sv.xError();}
const double xError(const reco::VertexCompositePtrCandidate & sv) {return sqrt(sv.vertexCovariance(0,0));}
const double yError(const reco::Vertex & sv) {return sv.yError();}
const double yError(const reco::VertexCompositePtrCandidate & sv) {return sqrt(sv.vertexCovariance(1,1));}
const double zError(const reco::Vertex & sv) {return sv.zError();}
const double zError(const reco::VertexCompositePtrCandidate & sv) {return sqrt(sv.vertexCovariance(2,2));}
const double chi2(const reco::Vertex & sv) {return sv.chi2();}
const double chi2(const reco::VertexCompositePtrCandidate & sv) {return sv.vertexChi2();}
const double ndof(const reco::Vertex & sv) {return sv.ndof();}
const double ndof(const reco::VertexCompositePtrCandidate & sv) {return sv.vertexNdof();}
const unsigned int nTracks(const reco::Vertex & sv) {return sv.nTracks();}
const unsigned int nTracks(const reco::VertexCompositePtrCandidate & sv) {return sv.numberOfSourceCandidatePtrs();}

const UInt_t MAX_JETCOLLECTIONS=2;

template<typename IPTI,typename VTX>
class BTagAnalyzerLiteT : public edm::EDAnalyzer
{
  public:
    explicit BTagAnalyzerLiteT(const edm::ParameterSet&);
    ~BTagAnalyzerLiteT();
    typedef IPTI IPTagInfo;
    typedef typename IPTI::input_container Tracks;
    typedef typename IPTI::input_container::value_type TrackRef;
    typedef VTX Vertex;
    typedef reco::TemplatedSecondaryVertexTagInfo<IPTI,VTX> SVTagInfo;

  private:
    virtual void beginJob() ;
    virtual void analyze(const edm::Event&, const edm::EventSetup&);
    virtual void endJob() ;

    const IPTagInfo * toIPTagInfo(const pat::Jet & jet, const std::string & tagInfos);
    const SVTagInfo * toSVTagInfo(const pat::Jet & jet, const std::string & tagInfos);

    void setTracksPVBase(const reco::TrackRef & trackRef, const edm::Handle<reco::VertexCollection> & pvHandle, int & iPV, float & PVweight);
    void setTracksPV(const TrackRef & trackRef, const edm::Handle<reco::VertexCollection> & pvHandle, int & iPV, float & PVweight);

    void setTracksSV(const TrackRef & trackRef, const SVTagInfo *, int & isFromSV, int & iSV, float & SVweight);

    void vertexKinematicsAndChange(const Vertex & vertex, reco::TrackKinematics & vertexKinematics, Int_t & charge);

    bool NameCompatible(const std::string& pattern, const std::string& name);

    void processTrig(const edm::Handle<edm::TriggerResults>&, const std::vector<std::string>&) ;

    void processJets(const edm::Handle<PatJetCollection>&, const edm::Handle<PatJetCollection>&,
                     const edm::Event&, const edm::EventSetup&,
                     const edm::Handle<PatJetCollection>&, std::vector<int>&, const int) ;

    void recalcNsubjettiness(const pat::Jet & jet, const SVTagInfo & svTagInfo, float & tau1, float & tau2);

    bool isHardProcess(const int status);

    void matchGroomedJets(const edm::Handle<PatJetCollection>& jets,
                          const edm::Handle<PatJetCollection>& matchedJets,
                          std::vector<int>& matchedIndices);

    // ----------member data ---------------------------
    std::string outputFile_;
    //std::vector< std::string > moduleLabel_;

    bool runSubJets_ ;
    bool allowJetSkipping_ ;
    bool storeEventInfo_;
    bool produceJetTrackTree_;
    bool produceJetPFLeptonTree_;
    bool storeMuonInfo_;
    bool storeTagVariables_;
    bool storeCSVTagVariables_;

    edm::InputTag src_;  // Generator/handronizer module label
    edm::InputTag muonCollectionName_;
    edm::InputTag prunedGenParticleCollectionName_;
    edm::InputTag triggerTable_;

    edm::InputTag JetCollectionTag_;
    edm::InputTag FatJetCollectionTag_;
    edm::InputTag GroomedFatJetCollectionTag_;

    edm::InputTag primaryVertexColl_;

    std::string jetPBJetTags_;
    std::string jetPNegBJetTags_;
    std::string jetPPosBJetTags_;

    std::string jetBPBJetTags_;
    std::string jetBPNegBJetTags_;
    std::string jetBPPosBJetTags_;

    std::string trackCHEBJetTags_;
    std::string trackCNegHEBJetTags_;

    std::string trackCHPBJetTags_;
    std::string trackCNegHPBJetTags_;

    std::string simpleSVHighEffBJetTags_;
    std::string simpleSVNegHighEffBJetTags_;
    std::string simpleSVHighPurBJetTags_;
    std::string simpleSVNegHighPurBJetTags_;

    std::string combinedSVBJetTags_;
    std::string combinedSVPosBJetTags_;
    std::string combinedSVNegBJetTags_;

    std::string combinedIVFSVBJetTags_;
    std::string combinedIVFSVPosBJetTags_;
    std::string combinedIVFSVNegBJetTags_;

    std::string softPFMuonBJetTags_;
    std::string softPFMuonNegBJetTags_;
    std::string softPFMuonPosBJetTags_;

    std::string softPFElectronBJetTags_;
    std::string softPFElectronNegBJetTags_;
    std::string softPFElectronPosBJetTags_;

    std::string ipTagInfos_;
    std::string svTagInfos_;
    std::string softPFMuonTagInfos_;
    std::string softPFElectronTagInfos_;

    std::string SVComputer_;
    std::string SVComputerFatJets_;

    TFile*  rootFile_;
    double minJetPt_;
    double maxJetEta_;

    bool isData_;

    // trigger list
    std::vector<std::string> triggerPathNames_;

    edm::Service<TFileService> fs;

    ///////////////
    // Ntuple info

    TTree *smalltree;

    //// Event info
    EventInfoBranches EventInfo;

    //// Jet info
    JetInfoBranches JetInfo[MAX_JETCOLLECTIONS] ;

    edm::Handle<reco::VertexCollection> primaryVertex;

    const reco::Vertex *pv;
    const GenericMVAJetTagComputer *computer;

    // Generator/hadronizer type (information stored bitwise)
    unsigned int hadronizerType_;

    // PF jet ID
    PFJetIDSelectionFunctor pfjetIDLoose_;
    PFJetIDSelectionFunctor pfjetIDTight_;

    // N-subjettiness calculator
    fastjet::contrib::Njettiness njettiness_;
};


template<typename IPTI,typename VTX>
BTagAnalyzerLiteT<IPTI,VTX>::BTagAnalyzerLiteT(const edm::ParameterSet& iConfig):
  pv(0),
  computer(0),
  hadronizerType_(0),
  pfjetIDLoose_( PFJetIDSelectionFunctor::FIRSTDATA, PFJetIDSelectionFunctor::LOOSE ),
  pfjetIDTight_( PFJetIDSelectionFunctor::FIRSTDATA, PFJetIDSelectionFunctor::TIGHT ),
  njettiness_(fastjet::contrib::OnePass_KT_Axes(), fastjet::contrib::NormalizedMeasure(1.0,0.8))
{
  //now do what ever initialization you need
  std::string module_type  = iConfig.getParameter<std::string>("@module_type");
  std::string module_label = iConfig.getParameter<std::string>("@module_label");
  std::cout << "Constructing " << module_type << ":" << module_label << std::endl;

  // Parameters
  runSubJets_ = iConfig.getParameter<bool>("runSubJets");
  allowJetSkipping_ = iConfig.getParameter<bool>("allowJetSkipping");
  storeEventInfo_ = iConfig.getParameter<bool>("storeEventInfo");
  produceJetTrackTree_  = iConfig.getParameter<bool> ("produceJetTrackTree");
  produceJetPFLeptonTree_  = iConfig.getParameter<bool> ("produceJetPFLeptonTree");
  storeMuonInfo_ = iConfig.getParameter<bool>("storeMuonInfo");
  storeTagVariables_ = iConfig.getParameter<bool>("storeTagVariables");
  storeCSVTagVariables_ = iConfig.getParameter<bool>("storeCSVTagVariables");
  minJetPt_  = iConfig.getParameter<double>("MinPt");
  maxJetEta_ = iConfig.getParameter<double>("MaxEta");

  // Modules
  src_                 = iConfig.getParameter<edm::InputTag>("src");
  muonCollectionName_       = iConfig.getParameter<edm::InputTag>("muonCollectionName");
  prunedGenParticleCollectionName_ = iConfig.getParameter<edm::InputTag>("prunedGenParticles");
  triggerTable_             = iConfig.getParameter<edm::InputTag>("triggerTable");

  JetCollectionTag_ = iConfig.getParameter<edm::InputTag>("Jets");
  FatJetCollectionTag_ = iConfig.getParameter<edm::InputTag>("FatJets");
  GroomedFatJetCollectionTag_ = iConfig.getParameter<edm::InputTag>("GroomedFatJets");

  primaryVertexColl_   = iConfig.getParameter<edm::InputTag>("primaryVertexColl");

  trackCHEBJetTags_    = iConfig.getParameter<std::string>("trackCHEBJetTags");
  trackCNegHEBJetTags_ = iConfig.getParameter<std::string>("trackCNegHEBJetTags");

  trackCHPBJetTags_    = iConfig.getParameter<std::string>("trackCHPBJetTags");
  trackCNegHPBJetTags_ = iConfig.getParameter<std::string>("trackCNegHPBJetTags");

  jetPBJetTags_        = iConfig.getParameter<std::string>("jetPBJetTags");
  jetPNegBJetTags_     = iConfig.getParameter<std::string>("jetPNegBJetTags");
  jetPPosBJetTags_     = iConfig.getParameter<std::string>("jetPPosBJetTags");

  jetBPBJetTags_        = iConfig.getParameter<std::string>("jetBPBJetTags");
  jetBPNegBJetTags_     = iConfig.getParameter<std::string>("jetBPNegBJetTags");
  jetBPPosBJetTags_     = iConfig.getParameter<std::string>("jetBPPosBJetTags");

  simpleSVHighEffBJetTags_      = iConfig.getParameter<std::string>("simpleSVHighEffBJetTags");
  simpleSVNegHighEffBJetTags_   = iConfig.getParameter<std::string>("simpleSVNegHighEffBJetTags");
  simpleSVHighPurBJetTags_      = iConfig.getParameter<std::string>("simpleSVHighPurBJetTags");
  simpleSVNegHighPurBJetTags_   = iConfig.getParameter<std::string>("simpleSVNegHighPurBJetTags");

  combinedSVBJetTags_     = iConfig.getParameter<std::string>("combinedSVBJetTags");
  combinedSVPosBJetTags_  = iConfig.getParameter<std::string>("combinedSVPosBJetTags");
  combinedSVNegBJetTags_  = iConfig.getParameter<std::string>("combinedSVNegBJetTags");

  combinedIVFSVBJetTags_      = iConfig.getParameter<std::string>("combinedIVFSVBJetTags");
  combinedIVFSVPosBJetTags_   = iConfig.getParameter<std::string>("combinedIVFSVPosBJetTags");
  combinedIVFSVNegBJetTags_   = iConfig.getParameter<std::string>("combinedIVFSVNegBJetTags");

  softPFMuonBJetTags_       = iConfig.getParameter<std::string>("softPFMuonBJetTags");
  softPFMuonNegBJetTags_    = iConfig.getParameter<std::string>("softPFMuonNegBJetTags");
  softPFMuonPosBJetTags_    = iConfig.getParameter<std::string>("softPFMuonPosBJetTags");

  softPFElectronBJetTags_       = iConfig.getParameter<std::string>("softPFElectronBJetTags");
  softPFElectronNegBJetTags_    = iConfig.getParameter<std::string>("softPFElectronNegBJetTags");
  softPFElectronPosBJetTags_    = iConfig.getParameter<std::string>("softPFElectronPosBJetTags");

  ipTagInfos_              = iConfig.getParameter<std::string>("ipTagInfos");
  svTagInfos_              = iConfig.getParameter<std::string>("svTagInfos");
  softPFMuonTagInfos_      = iConfig.getParameter<std::string>("softPFMuonTagInfos");
  softPFElectronTagInfos_  = iConfig.getParameter<std::string>("softPFElectronTagInfos");

  SVComputer_               = iConfig.getParameter<std::string>("svComputer");
  SVComputerFatJets_        = iConfig.getParameter<std::string>("svComputerFatJets");

  triggerPathNames_        = iConfig.getParameter<std::vector<std::string> >("TriggerPathNames");

  ///////////////
  // TTree

  smalltree = fs->make<TTree>("ttree", "ttree");

  //--------------------------------------
  // event information
  //--------------------------------------
  if( storeEventInfo_ )
  {
    EventInfo.RegisterTree(smalltree);
    if ( produceJetTrackTree_ ) EventInfo.RegisterJetTrackTree(smalltree);
  }
  if ( storeMuonInfo_ ) EventInfo.RegisterMuonTree(smalltree);

  //--------------------------------------
  // jet information
  //--------------------------------------
  JetInfo[0].RegisterTree(smalltree,(runSubJets_ ? "JetInfo" : ""));
  if ( runSubJets_ )          JetInfo[0].RegisterSubJetSpecificTree(smalltree,(runSubJets_ ? "JetInfo" : ""));
  if ( produceJetTrackTree_ ) JetInfo[0].RegisterJetTrackTree(smalltree,(runSubJets_ ? "JetInfo" : ""));
  if ( produceJetPFLeptonTree_ ) JetInfo[0].RegisterJetPFLeptonTree(smalltree,(runSubJets_ ? "JetInfo" : ""));
  if ( storeTagVariables_)    JetInfo[0].RegisterTagVarTree(smalltree,(runSubJets_ ? "JetInfo" : ""));
  if ( storeCSVTagVariables_) JetInfo[0].RegisterCSVTagVarTree(smalltree,(runSubJets_ ? "JetInfo" : ""));
  if ( runSubJets_ ) {
    JetInfo[1].RegisterTree(smalltree,"FatJetInfo");
    JetInfo[1].RegisterFatJetSpecificTree(smalltree,"FatJetInfo");
    if ( produceJetTrackTree_ ) JetInfo[1].RegisterJetTrackTree(smalltree,"FatJetInfo");
    if ( produceJetPFLeptonTree_ ) JetInfo[1].RegisterJetPFLeptonTree(smalltree,"FatJetInfo");
    if ( storeTagVariables_)    JetInfo[1].RegisterTagVarTree(smalltree,"FatJetInfo");
    if ( storeCSVTagVariables_) JetInfo[1].RegisterCSVTagVarTree(smalltree,"FatJetInfo");
  }

  std::cout << module_type << ":" << module_label << " constructed" << std::endl;
}

template<typename IPTI,typename VTX>
BTagAnalyzerLiteT<IPTI,VTX>::~BTagAnalyzerLiteT()
{
}


//
// member functions
//

// ------------ method called to for each event  ------------
template<typename IPTI,typename VTX>
void BTagAnalyzerLiteT<IPTI,VTX>::analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup)
{
  using namespace edm;
  using namespace std;
  using namespace reco;

  //------------------------------------------------------
  // Event information
  //------------------------------------------------------
  EventInfo.Run = iEvent.id().run();
  isData_ = iEvent.isRealData();

  if ( !isData_ && EventInfo.Run > 0 ) EventInfo.Run = -EventInfo.Run;

  EventInfo.Evt  = iEvent.id().event();
  EventInfo.LumiBlock  = iEvent.luminosityBlock();

  edm::Handle <PatJetCollection> jetsColl;
  iEvent.getByLabel (JetCollectionTag_, jetsColl);

  edm::Handle <PatJetCollection> fatjetsColl;
  edm::Handle <PatJetCollection> groomedfatjetsColl;
  if (runSubJets_) {
    iEvent.getByLabel(FatJetCollectionTag_, fatjetsColl) ;
    iEvent.getByLabel(GroomedFatJetCollectionTag_, groomedfatjetsColl) ;
  }

  // match groomed and original fat jets
  std::vector<int> groomedIndices;
  if (runSubJets_)
  {
    if( groomedfatjetsColl->size() > fatjetsColl->size() )
      edm::LogError("TooManyGroomedJets") << "There are more groomed (" << groomedfatjetsColl->size() << ") than original fat jets (" << fatjetsColl->size() << "). Please check that the two jet collections belong to each other.";

    matchGroomedJets(fatjetsColl,groomedfatjetsColl,groomedIndices);
  }

  //------------------------------------------------------
  // Determine hadronizer type (done only once per job)
  //------------------------------------------------------
  if( !isData_ && hadronizerType_ == 0 )
  {
    edm::Handle<GenEventInfoProduct> genEvtInfoProduct;
    iEvent.getByLabel(src_, genEvtInfoProduct);

    std::string moduleName = "";
    if( genEvtInfoProduct.isValid() )
    {
      const edm::Provenance& prov = iEvent.getProvenance(genEvtInfoProduct.id());
      moduleName = edm::moduleName(prov);
    }

    if( moduleName.find("Pythia8")!=std::string::npos )
      hadronizerType_ |= ( 1 << 1 ); // set the 2nd bit
    else // assuming Pythia6
      hadronizerType_ |= ( 1 << 0 ); // set the 1st bit
  }

  //------------------------------------------------------
  // MC informations
  //------------------------------------------------------
  EventInfo.pthat      = -1.;
  EventInfo.nPUtrue    = -1.;
  EventInfo.nPU        = 0;
  EventInfo.nGenPruned = 0;
  EventInfo.nMuon   = 0;
  EventInfo.mcweight   = 1.;

  //---------------------------- Start MC info ---------------------------------------//
  if ( !isData_ && storeEventInfo_ ) {
    // pthat
    edm::Handle<GenEventInfoProduct> geninfos;
    iEvent.getByLabel( "generator",geninfos );
    EventInfo.mcweight=geninfos->weight();
    if (geninfos->binningValues().size()>0) EventInfo.pthat = geninfos->binningValues()[0];

    // pileup
    edm::Handle<std::vector <PileupSummaryInfo> > PupInfo;
    iEvent.getByLabel("addPileupInfo", PupInfo);

    std::vector<PileupSummaryInfo>::const_iterator ipu;
    for (ipu = PupInfo->begin(); ipu != PupInfo->end(); ++ipu) {
      if ( ipu->getBunchCrossing() != 0 ) continue; // storing detailed PU info only for BX=0
      for (unsigned int i=0; i<ipu->getPU_zpositions().size(); ++i) {
        EventInfo.PU_bunch[EventInfo.nPU]      =  ipu->getBunchCrossing();
        EventInfo.PU_z[EventInfo.nPU]          = (ipu->getPU_zpositions())[i];
        EventInfo.PU_sumpT_low[EventInfo.nPU]  = (ipu->getPU_sumpT_lowpT())[i];
        EventInfo.PU_sumpT_high[EventInfo.nPU] = (ipu->getPU_sumpT_highpT())[i];
        EventInfo.PU_ntrks_low[EventInfo.nPU]  = (ipu->getPU_ntrks_lowpT())[i];
        EventInfo.PU_ntrks_high[EventInfo.nPU] = (ipu->getPU_ntrks_highpT())[i];
        ++EventInfo.nPU;
      }
      EventInfo.nPUtrue = ipu->getTrueNumInteractions();
      if(EventInfo.nPU==0) EventInfo.nPU = ipu->getPU_NumInteractions(); // needed in case getPU_zpositions() is empty
    }

    //------------------------------------------------------
    // pruned generated particles
    //------------------------------------------------------
    edm::Handle<reco::GenParticleCollection> prunedGenParticles;
    iEvent.getByLabel(prunedGenParticleCollectionName_, prunedGenParticles);

    EventInfo.GenPVz = -1000.;

    // loop over pruned GenParticles to fill branches for MC hard process particles and muons
    for(size_t i = 0; i < prunedGenParticles->size(); ++i){
      const GenParticle & iGenPart = (*prunedGenParticles)[i];
      int status = iGenPart.status();
      int pdgid = iGenPart.pdgId();
      int numMothers = iGenPart.numberOfMothers();

      if ( isHardProcess(iGenPart.status()) ) EventInfo.GenPVz = iGenPart.vz();

      //fill all the branches
      EventInfo.GenPruned_pT[EventInfo.nGenPruned] = iGenPart.pt();
      EventInfo.GenPruned_eta[EventInfo.nGenPruned] = iGenPart.eta();
      EventInfo.GenPruned_phi[EventInfo.nGenPruned] = iGenPart.phi();
      EventInfo.GenPruned_mass[EventInfo.nGenPruned] = iGenPart.mass();
      EventInfo.GenPruned_status[EventInfo.nGenPruned] = status;
      EventInfo.GenPruned_pdgID[EventInfo.nGenPruned] = pdgid;
      // if no mothers, set mother index to -1 (just so it's not >=0)
      if (numMothers == 0)
        EventInfo.GenPruned_mother[EventInfo.nGenPruned] = -1;
      else{
        //something new to distinguish from the no mothers case
        int idx = -100;
        //loop over the pruned genparticle list to get the mother's index
        for( reco::GenParticleCollection::const_iterator mit = prunedGenParticles->begin(); mit != prunedGenParticles->end(); ++mit ) {
          if( iGenPart.mother(0)==&(*mit) ) {
            idx = std::distance(prunedGenParticles->begin(),mit);
            break;
          }
        }
        EventInfo.GenPruned_mother[EventInfo.nGenPruned] = idx;
      }
      ++EventInfo.nGenPruned;
    } //end loop over pruned GenParticles
  }

  //---------------------------- End MC info ---------------------------------------//
  //   std::cout << "EventInfo.Evt:" <<EventInfo.Evt << std::endl;
  //   std::cout << "EventInfo.pthat:" <<EventInfo.pthat << std::endl;

  //------------------------------------------------------
  // Muons
  //------------------------------------------------------
  edm::Handle<std::vector<pat::Muon> >  muonsHandle;
  if( storeMuonInfo_ )
  {
    iEvent.getByLabel(muonCollectionName_,muonsHandle);

    for( std::vector<pat::Muon>::const_iterator it = muonsHandle->begin(); it != muonsHandle->end(); ++it )
    {
      if( !it->isGlobalMuon() ) continue;

      EventInfo.Muon_isGlobal[EventInfo.nMuon] = 1;
      EventInfo.Muon_isPF[EventInfo.nMuon]     = it->isPFMuon();
      EventInfo.Muon_nTkHit[EventInfo.nMuon]   = it->innerTrack()->hitPattern().numberOfValidHits();
      EventInfo.Muon_nPixHit[EventInfo.nMuon]  = it->innerTrack()->hitPattern().numberOfValidPixelHits();
      EventInfo.Muon_nOutHit[EventInfo.nMuon]  = it->innerTrack()->hitPattern().numberOfHits(reco::HitPattern::MISSING_OUTER_HITS);
      EventInfo.Muon_nMuHit[EventInfo.nMuon]   = it->outerTrack()->hitPattern().numberOfValidMuonHits();
      EventInfo.Muon_nMatched[EventInfo.nMuon] = it->numberOfMatches();
      EventInfo.Muon_chi2[EventInfo.nMuon]     = it->globalTrack()->normalizedChi2();
      EventInfo.Muon_chi2Tk[EventInfo.nMuon]   = it->innerTrack()->normalizedChi2();
      EventInfo.Muon_pt[EventInfo.nMuon]       = it->pt();
      EventInfo.Muon_eta[EventInfo.nMuon]      = it->eta();
      EventInfo.Muon_phi[EventInfo.nMuon]      = it->phi();
      EventInfo.Muon_vz[EventInfo.nMuon]       = it->vz();
      EventInfo.Muon_IP[EventInfo.nMuon]       = it->dB(pat::Muon::PV3D);
      EventInfo.Muon_IPsig[EventInfo.nMuon]    = (it->dB(pat::Muon::PV3D))/(it->edB(pat::Muon::PV3D));
      EventInfo.Muon_IP2D[EventInfo.nMuon]     = it->dB(pat::Muon::PV2D);
      EventInfo.Muon_IP2Dsig[EventInfo.nMuon]  = (it->dB(pat::Muon::PV2D))/(it->edB(pat::Muon::PV2D));

      ++EventInfo.nMuon;
    }
  }

  //------------------
  // Primary vertex
  //------------------
  iEvent.getByLabel(primaryVertexColl_,primaryVertex);

  bool pvFound = (primaryVertex->size() != 0);
  if ( pvFound ) {
    pv = &(*primaryVertex->begin());
  }
  else {
    reco::Vertex::Error e;
    e(0,0)=0.0015*0.0015;
    e(1,1)=0.0015*0.0015;
    e(2,2)=15.*15.;
    reco::Vertex::Point p(0,0,0);
    pv=  new reco::Vertex(p,e,1,1,1);
  }
  //   GlobalPoint Pv_point = GlobalPoint((*pv).x(), (*pv).y(), (*pv).z());
  EventInfo.PVz = (*primaryVertex)[0].z();
  EventInfo.PVez = (*primaryVertex)[0].zError();

  EventInfo.nPV=0;
  for (unsigned int i = 0; i< primaryVertex->size() ; ++i) {
    EventInfo.PV_x[EventInfo.nPV]      = (*primaryVertex)[i].x();
    EventInfo.PV_y[EventInfo.nPV]      = (*primaryVertex)[i].y();
    EventInfo.PV_z[EventInfo.nPV]      = (*primaryVertex)[i].z();
    EventInfo.PV_ex[EventInfo.nPV]     = (*primaryVertex)[i].xError();
    EventInfo.PV_ey[EventInfo.nPV]     = (*primaryVertex)[i].yError();
    EventInfo.PV_ez[EventInfo.nPV]     = (*primaryVertex)[i].zError();
    EventInfo.PV_chi2[EventInfo.nPV]   = (*primaryVertex)[i].normalizedChi2();
    EventInfo.PV_ndf[EventInfo.nPV]    = (*primaryVertex)[i].ndof();
    EventInfo.PV_isgood[EventInfo.nPV] = (*primaryVertex)[i].isValid();
    EventInfo.PV_isfake[EventInfo.nPV] = (*primaryVertex)[i].isFake();

    ++EventInfo.nPV;
  }

  //------------------------------------------------------
  // Trigger info
  //------------------------------------------------------

  edm::Handle<edm::TriggerResults> trigRes;
  iEvent.getByLabel(triggerTable_, trigRes);

  EventInfo.nBitTrigger = int(triggerPathNames_.size()/32)+1;
  for(int i=0; i<EventInfo.nBitTrigger; ++i) EventInfo.BitTrigger[i] = 0;

  std::vector<std::string> triggerList;
  edm::Service<edm::service::TriggerNamesService> tns;
  bool foundNames = tns->getTrigPaths(*trigRes,triggerList);
  if ( !foundNames ) edm::LogError("TriggerNamesNotFound") << "Could not get trigger names!";
  if ( trigRes->size() != triggerList.size() ) edm::LogError("TriggerPathLengthMismatch") << "Length of names and paths not the same: "
    << triggerList.size() << "," << trigRes->size() ;

  processTrig(trigRes, triggerList);

  //------------- added by Camille-----------------------------------------------------------//
  edm::ESHandle<JetTagComputer> computerHandle;
  iSetup.get<JetTagComputerRecord>().get( SVComputer_.c_str(), computerHandle );

  computer = dynamic_cast<const GenericMVAJetTagComputer*>( computerHandle.product() );
  //------------- end added-----------------------------------------------------------//

  //------------------------------------------------------
  // Jet info
  //------------------------------------------------------
  int iJetColl = 0 ;
  //// Do jets
  processJets(jetsColl, fatjetsColl, iEvent, iSetup, groomedfatjetsColl, groomedIndices, iJetColl) ;
  if (runSubJets_) {
    iJetColl = 1 ;
    // for fat jets we might have a different jet tag computer
    // if so, grab the fat jet tag computer
    if(SVComputerFatJets_!=SVComputer_)
    {
      iSetup.get<JetTagComputerRecord>().get( SVComputerFatJets_.c_str(), computerHandle );

      computer = dynamic_cast<const GenericMVAJetTagComputer*>( computerHandle.product() );
    }
    processJets(fatjetsColl, jetsColl, iEvent, iSetup, groomedfatjetsColl, groomedIndices, iJetColl) ;
  }
  //------------------------------------------------------

  //// Fill TTree
  if ( EventInfo.BitTrigger > 0 || EventInfo.Run < 0 ) {
    smalltree->Fill();
  }

  return;
}


template<typename IPTI,typename VTX>
void BTagAnalyzerLiteT<IPTI,VTX>::processTrig(const edm::Handle<edm::TriggerResults>& trigRes, const std::vector<std::string>& triggerList)
{
  for (unsigned int i = 0; i < trigRes->size(); ++i) {

    if ( !trigRes->at(i).accept() ) continue;


    for (std::vector<std::string>::const_iterator itTrigPathNames = triggerPathNames_.begin();
        itTrigPathNames != triggerPathNames_.end(); ++itTrigPathNames)
    {
      int triggerIdx = ( itTrigPathNames - triggerPathNames_.begin() );
      int bitIdx = int(triggerIdx/32);
      if ( NameCompatible(*itTrigPathNames,triggerList[i]) ) EventInfo.BitTrigger[bitIdx] |= ( 1 << (triggerIdx - bitIdx*32) );
    }
  } //// Loop over trigger names

  return;
}


template<typename IPTI,typename VTX>
void BTagAnalyzerLiteT<IPTI,VTX>::processJets(const edm::Handle<PatJetCollection>& jetsColl, const edm::Handle<PatJetCollection>& jetsColl2,
                               const edm::Event& iEvent, const edm::EventSetup& iSetup,
                               const edm::Handle<PatJetCollection>& jetsColl3, std::vector<int>& jetIndices, const int iJetColl)
{

  JetInfo[iJetColl].nPFElectron = 0;
  JetInfo[iJetColl].nPFMuon = 0;

  JetInfo[iJetColl].nJet = 0;
  JetInfo[iJetColl].nTrack = 0;
  JetInfo[iJetColl].nSV = 0;
  JetInfo[iJetColl].nTrkTagVar = 0;
  JetInfo[iJetColl].nSVTagVar = 0;
  JetInfo[iJetColl].nTrkTagVarCSV = 0;
  JetInfo[iJetColl].nTrkEtaRelTagVarCSV = 0;
  JetInfo[iJetColl].nSubJet = 0;

  //// Loop over the jets
  for ( PatJetCollection::const_iterator pjet = jetsColl->begin(); pjet != jetsColl->end(); ++pjet ) {

    double ptjet  = pjet->pt()  ;
    double etajet = pjet->eta() ;
    //double phijet = pjet->phi() ;

    if ( allowJetSkipping_ && ( ptjet < minJetPt_ || std::fabs( etajet ) > maxJetEta_ ) ) continue;

    int flavour  =-1  ;
    if ( !isData_ ) {
      flavour = abs( pjet->partonFlavour() );
      if ( flavour >= 1 && flavour <= 3 ) flavour = 1;
    }

    JetInfo[iJetColl].Jet_flavour[JetInfo[iJetColl].nJet]   = pjet->partonFlavour();
    JetInfo[iJetColl].Jet_nbHadrons[JetInfo[iJetColl].nJet] = pjet->jetFlavourInfo().getbHadrons().size();
    JetInfo[iJetColl].Jet_ncHadrons[JetInfo[iJetColl].nJet] = pjet->jetFlavourInfo().getcHadrons().size();
    JetInfo[iJetColl].Jet_eta[JetInfo[iJetColl].nJet]       = pjet->eta();
    JetInfo[iJetColl].Jet_phi[JetInfo[iJetColl].nJet]       = pjet->phi();
    JetInfo[iJetColl].Jet_pt[JetInfo[iJetColl].nJet]        = pjet->pt();
    JetInfo[iJetColl].Jet_mass[JetInfo[iJetColl].nJet]      = pjet->mass();
    JetInfo[iJetColl].Jet_genpt[JetInfo[iJetColl].nJet]     = ( pjet->genJet()!=0 ? pjet->genJet()->pt() : -1. );

    // available JEC sets
    unsigned int nJECSets = pjet->availableJECSets().size();
    // PF jet ID
    pat::strbitset retpf = pfjetIDLoose_.getBitTemplate();
    retpf.set(false);
    JetInfo[iJetColl].Jet_looseID[JetInfo[iJetColl].nJet]  = ( ( nJECSets>0 && pjet->isPFJet() ) ? ( pfjetIDLoose_( *pjet, retpf ) ? 1 : 0 ) : 0 );
    retpf.set(false);
    JetInfo[iJetColl].Jet_tightID[JetInfo[iJetColl].nJet]  = ( ( nJECSets>0 && pjet->isPFJet() ) ? ( pfjetIDTight_( *pjet, retpf ) ? 1 : 0 ) : 0 );

    JetInfo[iJetColl].Jet_jes[JetInfo[iJetColl].nJet]      = ( nJECSets>0 ? pjet->pt()/pjet->correctedJet("Uncorrected").pt() : 1. );
    JetInfo[iJetColl].Jet_residual[JetInfo[iJetColl].nJet] = ( nJECSets>0 ? pjet->pt()/pjet->correctedJet("L3Absolute").pt() : 1. );

    if( runSubJets_ && iJetColl == 0 )
    {
      int fatjetIdx=-1;
      bool fatJetFound = false;
      // loop over fat jets
      for( int fjIt = 0; fjIt < (int)jetIndices.size(); ++fjIt )
      {
        int gfjIdx = jetIndices.at(fjIt);
        if( gfjIdx < 0 ) // skip fat jets with no matching groomed fat jet
          continue;

        int nSJ = jetsColl3->at(gfjIdx).numberOfDaughters();
        const edm::Ptr<reco::Jet> originalObjRef = edm::Ptr<reco::Jet>( jetsColl3->at(gfjIdx).originalObjectRef() );
        // loop over subjets
        for( int sjIt = 0; sjIt < nSJ; ++sjIt )
        {
          if( pjet->originalObjectRef() == originalObjRef->daughterPtr(sjIt) )
          {
            fatjetIdx = fjIt;
            fatJetFound = true;
            break;
          }
        }
        if( fatJetFound )
          break;
      }
      JetInfo[iJetColl].Jet_FatJetIdx[JetInfo[iJetColl].nJet] = fatjetIdx;

      if( ptjet==0. ) // special treatment for pT=0 subjets
      {
        ++JetInfo[iJetColl].nJet;
        continue;
      }
    }

    int subjet1Idx = -1, subjet2Idx = -1;

    if ( runSubJets_ && iJetColl == 1 )
    {
      // N-subjettiness
      JetInfo[iJetColl].Jet_tau1[JetInfo[iJetColl].nJet] = pjet->userFloat("Njettiness:tau1");
      JetInfo[iJetColl].Jet_tau2[JetInfo[iJetColl].nJet] = pjet->userFloat("Njettiness:tau2");

      int gfjIdx = jetIndices.at( pjet - jetsColl->begin() );
      int nSJ = 0;
      JetInfo[iJetColl].Jet_nFirstSJ[JetInfo[iJetColl].nJet] = JetInfo[iJetColl].nSubJet;
      std::vector<PatJetCollection::const_iterator> subjetIters;

      if ( gfjIdx >= 0 )
      {
        nSJ = jetsColl3->at(gfjIdx).numberOfDaughters();

        JetInfo[iJetColl].Jet_ptGroomed[JetInfo[iJetColl].nJet]   = jetsColl3->at(gfjIdx).pt();
        JetInfo[iJetColl].Jet_jesGroomed[JetInfo[iJetColl].nJet]  = jetsColl3->at(gfjIdx).pt()/jetsColl3->at(gfjIdx).correctedJet("Uncorrected").pt();
        JetInfo[iJetColl].Jet_etaGroomed[JetInfo[iJetColl].nJet]  = jetsColl3->at(gfjIdx).eta();
        JetInfo[iJetColl].Jet_phiGroomed[JetInfo[iJetColl].nJet]  = jetsColl3->at(gfjIdx).phi();
        JetInfo[iJetColl].Jet_massGroomed[JetInfo[iJetColl].nJet] = jetsColl3->at(gfjIdx).mass();

        const edm::Ptr<reco::Jet> originalObjRef = edm::Ptr<reco::Jet>( jetsColl3->at(gfjIdx).originalObjectRef() );
        // loop over subjets
        for( int sjIt = 0; sjIt < nSJ; ++sjIt )
        {
          JetInfo[iJetColl].SubJetIdx[JetInfo[iJetColl].nSubJet] = -1;

          for( PatJetCollection::const_iterator jIt = jetsColl2->begin(); jIt != jetsColl2->end(); ++jIt )
          {
            if( jIt->originalObjectRef() == originalObjRef->daughterPtr(sjIt) )
            {
              JetInfo[iJetColl].SubJetIdx[JetInfo[iJetColl].nSubJet] = ( jIt - jetsColl2->begin() );
              subjetIters.push_back( jIt );
              break;
            }
          }
          ++JetInfo[iJetColl].nSubJet;
        }
      }
      else
      {
        JetInfo[iJetColl].Jet_ptGroomed[JetInfo[iJetColl].nJet]   = -9999;
        JetInfo[iJetColl].Jet_jesGroomed[JetInfo[iJetColl].nJet]  = -9999;
        JetInfo[iJetColl].Jet_etaGroomed[JetInfo[iJetColl].nJet]  = -9999;
        JetInfo[iJetColl].Jet_phiGroomed[JetInfo[iJetColl].nJet]  = -9999;
        JetInfo[iJetColl].Jet_massGroomed[JetInfo[iJetColl].nJet] = -9999;
      }
      JetInfo[iJetColl].Jet_nSubJets[JetInfo[iJetColl].nJet] = nSJ;
      JetInfo[iJetColl].Jet_nLastSJ[JetInfo[iJetColl].nJet] = JetInfo[iJetColl].nSubJet;

      // sort subjets by uncorrected Pt
      std::sort(subjetIters.begin(), subjetIters.end(), orderByPt("Uncorrected"));
      // take two leading subjets
      if( subjetIters.size()>1 )
      {
         subjet1Idx = ( subjetIters.at(0) - jetsColl2->begin() );
         subjet2Idx = ( subjetIters.at(1) - jetsColl2->begin() );
      }

      int nsubjettracks = 0, nsharedsubjettracks = 0;

      if( subjet1Idx>=0 && subjet2Idx>=0 ) // protection for pathological cases of groomed fat jets with only one constituent which results in an undefined subjet 2 index
      {
        for(int sj=0; sj<2; ++sj)
        {
          int subjetIdx = (sj==0 ? subjet1Idx : subjet2Idx); // subjet index
          int compSubjetIdx = (sj==0 ? subjet2Idx : subjet1Idx); // companion subjet index
          int nTracks = ( jetsColl2->at(subjetIdx).hasTagInfo(ipTagInfos_.c_str()) ? toIPTagInfo(jetsColl2->at(subjetIdx),ipTagInfos_)->selectedTracks().size() : 0 );

          for(int t=0; t<nTracks; ++t)
          {
            if( reco::deltaR( toIPTagInfo(jetsColl2->at(subjetIdx),ipTagInfos_)->selectedTracks().at(t)->eta(), toIPTagInfo(jetsColl2->at(subjetIdx),ipTagInfos_)->selectedTracks().at(t)->phi(), jetsColl2->at(subjetIdx).eta(), jetsColl2->at(subjetIdx).phi() ) < 0.3 )
            {
              ++nsubjettracks;
              if( reco::deltaR( toIPTagInfo(jetsColl2->at(subjetIdx),ipTagInfos_)->selectedTracks().at(t)->eta(), toIPTagInfo(jetsColl2->at(subjetIdx),ipTagInfos_)->selectedTracks().at(t)->phi(), jetsColl2->at(compSubjetIdx).eta(), jetsColl2->at(compSubjetIdx).phi() ) < 0.3 )
              {
                if(sj==0) ++nsharedsubjettracks;
              }
            }
          }
        }
      }

      JetInfo[iJetColl].Jet_nsubjettracks[JetInfo[iJetColl].nJet] = nsubjettracks-nsharedsubjettracks;
      JetInfo[iJetColl].Jet_nsharedsubjettracks[JetInfo[iJetColl].nJet] = nsharedsubjettracks;
    }

    // Get all TagInfo pointers
    const IPTagInfo *ipTagInfo = toIPTagInfo(*pjet,ipTagInfos_);
    const SVTagInfo *svTagInfo = toSVTagInfo(*pjet,svTagInfos_);
    const reco::CandSoftLeptonTagInfo *softPFMuTagInfo = pjet->tagInfoCandSoftLepton(softPFMuonTagInfos_.c_str());
    const reco::CandSoftLeptonTagInfo *softPFElTagInfo = pjet->tagInfoCandSoftLepton(softPFElectronTagInfos_.c_str());

    // Re-calculate N-subjettiness using IVF vertices as composite b candidates
    if ( runSubJets_ && iJetColl == 1 )
    {
      float tau1IVF = JetInfo[iJetColl].Jet_tau1[JetInfo[iJetColl].nJet];
      float tau2IVF = JetInfo[iJetColl].Jet_tau2[JetInfo[iJetColl].nJet];

      // re-calculate N-subjettiness
      recalcNsubjettiness(*pjet,*svTagInfo,tau1IVF,tau2IVF);

      // store re-calculated N-subjettiness
      JetInfo[iJetColl].Jet_tau1IVF[JetInfo[iJetColl].nJet] = tau1IVF;
      JetInfo[iJetColl].Jet_tau2IVF[JetInfo[iJetColl].nJet] = tau2IVF;
    }

    //*****************************************************************
    // Taggers
    //*****************************************************************

    // Loop on Selected Tracks
    JetInfo[iJetColl].Jet_ntracks[JetInfo[iJetColl].nJet] = 0;

    int nseltracks = 0;
    int nsharedtracks = 0;
    reco::TrackKinematics allKinematics;
	

    if ( produceJetTrackTree_ )
    {
      const Tracks & selectedTracks( ipTagInfo->selectedTracks() );

      JetInfo[iJetColl].Jet_ntracks[JetInfo[iJetColl].nJet] = selectedTracks.size();

      JetInfo[iJetColl].Jet_nFirstTrack[JetInfo[iJetColl].nJet] = JetInfo[iJetColl].nTrack;

      unsigned int trackSize = selectedTracks.size();

      for (unsigned int itt=0; itt < trackSize; ++itt)
      {
        const reco::Track & ptrack = *(reco::btag::toTrack(selectedTracks[itt]));
        const TrackRef ptrackRef = selectedTracks[itt];

        //--------------------------------
        float decayLength = (ipTagInfo->impactParameterData()[itt].closestToJetAxis - RecoVertex::convertPos(pv->position())).mag();
        float distJetAxis = ipTagInfo->impactParameterData()[itt].distanceToJetAxis.value();

        JetInfo[iJetColl].Track_dist[JetInfo[iJetColl].nTrack]     = distJetAxis;
        JetInfo[iJetColl].Track_length[JetInfo[iJetColl].nTrack]   = decayLength;

        JetInfo[iJetColl].Track_dxy[JetInfo[iJetColl].nTrack]      = ptrack.dxy(pv->position());
        JetInfo[iJetColl].Track_dz[JetInfo[iJetColl].nTrack]       = ptrack.dz(pv->position());
        JetInfo[iJetColl].Track_zIP[JetInfo[iJetColl].nTrack]      = ptrack.dz()-(*pv).z();

        float deltaR = reco::deltaR( ptrack.eta(), ptrack.phi(),
                                     JetInfo[iJetColl].Jet_eta[JetInfo[iJetColl].nJet], JetInfo[iJetColl].Jet_phi[JetInfo[iJetColl].nJet] );

        if (deltaR < 0.3) nseltracks++;

        if ( runSubJets_ && iJetColl == 1 && subjet1Idx >= 0 && subjet2Idx >= 0 ) {

          float dR1 = reco::deltaR( ptrack.eta(), ptrack.phi(),
                                    JetInfo[0].Jet_eta[subjet1Idx], JetInfo[0].Jet_phi[subjet1Idx] );

          float dR2 = reco::deltaR( ptrack.eta(), ptrack.phi(),
                                    JetInfo[0].Jet_eta[subjet2Idx], JetInfo[0].Jet_phi[subjet2Idx] );

          if ( dR1 < 0.3 && dR2 < 0.3 ) nsharedtracks++;
        }

        JetInfo[iJetColl].Track_IP2D[JetInfo[iJetColl].nTrack]     = ipTagInfo->impactParameterData()[itt].ip2d.value();
        JetInfo[iJetColl].Track_IP2Dsig[JetInfo[iJetColl].nTrack]  = ipTagInfo->impactParameterData()[itt].ip2d.significance();
        JetInfo[iJetColl].Track_IP[JetInfo[iJetColl].nTrack]       = ipTagInfo->impactParameterData()[itt].ip3d.value();
        JetInfo[iJetColl].Track_IPsig[JetInfo[iJetColl].nTrack]    = ipTagInfo->impactParameterData()[itt].ip3d.significance();
        JetInfo[iJetColl].Track_IP2Derr[JetInfo[iJetColl].nTrack]  = ipTagInfo->impactParameterData()[itt].ip2d.error();
        JetInfo[iJetColl].Track_IPerr[JetInfo[iJetColl].nTrack]    = ipTagInfo->impactParameterData()[itt].ip3d.error();
        JetInfo[iJetColl].Track_Proba[JetInfo[iJetColl].nTrack]    = ipTagInfo->probabilities(0)[itt];

        JetInfo[iJetColl].Track_p[JetInfo[iJetColl].nTrack]        = ptrack.p();
        JetInfo[iJetColl].Track_pt[JetInfo[iJetColl].nTrack]       = ptrack.pt();
        JetInfo[iJetColl].Track_eta[JetInfo[iJetColl].nTrack]      = ptrack.eta();
        JetInfo[iJetColl].Track_phi[JetInfo[iJetColl].nTrack]      = ptrack.phi();
        JetInfo[iJetColl].Track_chi2[JetInfo[iJetColl].nTrack]     = ptrack.normalizedChi2();
        JetInfo[iJetColl].Track_charge[JetInfo[iJetColl].nTrack]   = ptrack.charge();

        JetInfo[iJetColl].Track_nHitAll[JetInfo[iJetColl].nTrack]  = ptrack.numberOfValidHits();
        JetInfo[iJetColl].Track_nHitPixel[JetInfo[iJetColl].nTrack]= ptrack.hitPattern().numberOfValidPixelHits();
        JetInfo[iJetColl].Track_nHitStrip[JetInfo[iJetColl].nTrack]= ptrack.hitPattern().numberOfValidStripHits();
        JetInfo[iJetColl].Track_nHitTIB[JetInfo[iJetColl].nTrack]  = ptrack.hitPattern().numberOfValidStripTIBHits();
        JetInfo[iJetColl].Track_nHitTID[JetInfo[iJetColl].nTrack]  = ptrack.hitPattern().numberOfValidStripTIDHits();
        JetInfo[iJetColl].Track_nHitTOB[JetInfo[iJetColl].nTrack]  = ptrack.hitPattern().numberOfValidStripTOBHits();
        JetInfo[iJetColl].Track_nHitTEC[JetInfo[iJetColl].nTrack]  = ptrack.hitPattern().numberOfValidStripTECHits();
        JetInfo[iJetColl].Track_nHitPXB[JetInfo[iJetColl].nTrack]  = ptrack.hitPattern().numberOfValidPixelBarrelHits();
        JetInfo[iJetColl].Track_nHitPXF[JetInfo[iJetColl].nTrack]  = ptrack.hitPattern().numberOfValidPixelEndcapHits();
        JetInfo[iJetColl].Track_isHitL1[JetInfo[iJetColl].nTrack]  = ptrack.hitPattern().hasValidHitInFirstPixelBarrel();

        setTracksPV(ptrackRef, primaryVertex,
                    JetInfo[iJetColl].Track_PV[JetInfo[iJetColl].nTrack],
                    JetInfo[iJetColl].Track_PVweight[JetInfo[iJetColl].nTrack]);
	
	if(JetInfo[iJetColl].Track_PVweight[JetInfo[iJetColl].nTrack]>0) { allKinematics.add(ptrack, JetInfo[iJetColl].Track_PVweight[JetInfo[iJetColl].nTrack]); }

        if( pjet->hasTagInfo(svTagInfos_.c_str()) )
        {
          setTracksSV(ptrackRef, svTagInfo,
                      JetInfo[iJetColl].Track_isfromSV[JetInfo[iJetColl].nTrack],
                      JetInfo[iJetColl].Track_SV[JetInfo[iJetColl].nTrack],
                      JetInfo[iJetColl].Track_SVweight[JetInfo[iJetColl].nTrack]);
        }
        else
        {
          JetInfo[iJetColl].Track_isfromSV[JetInfo[iJetColl].nTrack] = 0;
          JetInfo[iJetColl].Track_SV[JetInfo[iJetColl].nTrack] = -1;
          JetInfo[iJetColl].Track_SVweight[JetInfo[iJetColl].nTrack] = 0.;
        }

        ++JetInfo[iJetColl].nTrack;
      } //// end loop on tracks
    }

    JetInfo[iJetColl].Jet_nseltracks[JetInfo[iJetColl].nJet] = nseltracks;

    if ( runSubJets_ && iJetColl == 1 )
      JetInfo[iJetColl].Jet_nsharedtracks[JetInfo[iJetColl].nJet] = nsharedtracks;

    JetInfo[iJetColl].Jet_nLastTrack[JetInfo[iJetColl].nJet]   = JetInfo[iJetColl].nTrack;

    if ( produceJetPFLeptonTree_ )
    {
      // PFMuon information
      for (unsigned int leptIdx = 0; leptIdx < (pjet->hasTagInfo(softPFMuonTagInfos_.c_str()) ? softPFMuTagInfo->leptons() : 0); ++leptIdx) {

        JetInfo[iJetColl].PFMuon_IdxJet[JetInfo[iJetColl].nPFMuon]    = JetInfo[iJetColl].nJet;
        JetInfo[iJetColl].PFMuon_pt[JetInfo[iJetColl].nPFMuon]        = softPFMuTagInfo->lepton(leptIdx)->pt();
        JetInfo[iJetColl].PFMuon_eta[JetInfo[iJetColl].nPFMuon]       = softPFMuTagInfo->lepton(leptIdx)->eta();
        JetInfo[iJetColl].PFMuon_phi[JetInfo[iJetColl].nPFMuon]       = softPFMuTagInfo->lepton(leptIdx)->phi();
        JetInfo[iJetColl].PFMuon_ptrel[JetInfo[iJetColl].nPFMuon]     = (softPFMuTagInfo->properties(leptIdx).ptRel);
        JetInfo[iJetColl].PFMuon_ratio[JetInfo[iJetColl].nPFMuon]     = (softPFMuTagInfo->properties(leptIdx).ratio);
        JetInfo[iJetColl].PFMuon_ratioRel[JetInfo[iJetColl].nPFMuon]  = (softPFMuTagInfo->properties(leptIdx).ratioRel);
        JetInfo[iJetColl].PFMuon_deltaR[JetInfo[iJetColl].nPFMuon]    = (softPFMuTagInfo->properties(leptIdx).deltaR);
        JetInfo[iJetColl].PFMuon_IP[JetInfo[iJetColl].nPFMuon]        = (softPFMuTagInfo->properties(leptIdx).sip3d);
        JetInfo[iJetColl].PFMuon_IP2D[JetInfo[iJetColl].nPFMuon]      = (softPFMuTagInfo->properties(leptIdx).sip2d);

        ++JetInfo[iJetColl].nPFMuon;
      }

      // PFElectron information
      for (unsigned int leptIdx = 0; leptIdx < (pjet->hasTagInfo(softPFElectronTagInfos_.c_str()) ? softPFElTagInfo->leptons() : 0); ++leptIdx) {

        JetInfo[iJetColl].PFElectron_IdxJet[JetInfo[iJetColl].nPFElectron]    = JetInfo[iJetColl].nJet;
        JetInfo[iJetColl].PFElectron_pt[JetInfo[iJetColl].nPFElectron]        = softPFElTagInfo->lepton(leptIdx)->pt();
        JetInfo[iJetColl].PFElectron_eta[JetInfo[iJetColl].nPFElectron]       = softPFElTagInfo->lepton(leptIdx)->eta();
        JetInfo[iJetColl].PFElectron_phi[JetInfo[iJetColl].nPFElectron]       = softPFElTagInfo->lepton(leptIdx)->phi();
        JetInfo[iJetColl].PFElectron_ptrel[JetInfo[iJetColl].nPFElectron]     = (softPFElTagInfo->properties(leptIdx).ptRel);
        JetInfo[iJetColl].PFElectron_ratio[JetInfo[iJetColl].nPFElectron]     = (softPFElTagInfo->properties(leptIdx).ratio);
        JetInfo[iJetColl].PFElectron_ratioRel[JetInfo[iJetColl].nPFElectron]  = (softPFElTagInfo->properties(leptIdx).ratioRel);
        JetInfo[iJetColl].PFElectron_deltaR[JetInfo[iJetColl].nPFElectron]    = (softPFElTagInfo->properties(leptIdx).deltaR);
        JetInfo[iJetColl].PFElectron_IP[JetInfo[iJetColl].nPFElectron]        = (softPFElTagInfo->properties(leptIdx).sip3d);
        JetInfo[iJetColl].PFElectron_IP2D[JetInfo[iJetColl].nPFElectron]      = (softPFElTagInfo->properties(leptIdx).sip2d);

        ++JetInfo[iJetColl].nPFElectron;
      }
    }

    // b-tagger discriminants
    float Proba  = pjet->bDiscriminator(jetPBJetTags_.c_str());
    float ProbaN = pjet->bDiscriminator(jetPNegBJetTags_.c_str());
    float ProbaP = pjet->bDiscriminator(jetPPosBJetTags_.c_str());

    float Bprob  = pjet->bDiscriminator(jetBPBJetTags_.c_str());
    float BprobN = pjet->bDiscriminator(jetBPNegBJetTags_.c_str());
    float BprobP = pjet->bDiscriminator(jetBPPosBJetTags_.c_str());

    float Svtx    = pjet->bDiscriminator(simpleSVHighEffBJetTags_.c_str());
    float SvtxN   = pjet->bDiscriminator(simpleSVNegHighEffBJetTags_.c_str());
    float SvtxHP  = pjet->bDiscriminator(simpleSVHighPurBJetTags_.c_str());
    float SvtxNHP = pjet->bDiscriminator(simpleSVNegHighPurBJetTags_.c_str());

    float CombinedSvtx  = pjet->bDiscriminator(combinedSVBJetTags_.c_str());
    float CombinedSvtxP = pjet->bDiscriminator(combinedSVPosBJetTags_.c_str());
    float CombinedSvtxN = pjet->bDiscriminator(combinedSVNegBJetTags_.c_str());

    float CombinedIVF     = pjet->bDiscriminator(combinedIVFSVBJetTags_.c_str());
    float CombinedIVF_P   = pjet->bDiscriminator(combinedIVFSVPosBJetTags_.c_str());
    float CombinedIVF_N   = pjet->bDiscriminator(combinedIVFSVNegBJetTags_.c_str());

    float SoftM  = pjet->bDiscriminator(softPFMuonBJetTags_.c_str());
    float SoftMN = pjet->bDiscriminator(softPFMuonNegBJetTags_.c_str());
    float SoftMP = pjet->bDiscriminator(softPFMuonPosBJetTags_.c_str());

    float SoftE  = pjet->bDiscriminator(softPFElectronBJetTags_.c_str());
    float SoftEN = pjet->bDiscriminator(softPFElectronNegBJetTags_.c_str());
    float SoftEP = pjet->bDiscriminator(softPFElectronPosBJetTags_.c_str());

    // Jet information
    JetInfo[iJetColl].Jet_ProbaN[JetInfo[iJetColl].nJet]   = ProbaN;
    JetInfo[iJetColl].Jet_ProbaP[JetInfo[iJetColl].nJet]   = ProbaP;
    JetInfo[iJetColl].Jet_Proba[JetInfo[iJetColl].nJet]    = Proba;
    JetInfo[iJetColl].Jet_BprobN[JetInfo[iJetColl].nJet]   = BprobN;
    JetInfo[iJetColl].Jet_BprobP[JetInfo[iJetColl].nJet]   = BprobP;
    JetInfo[iJetColl].Jet_Bprob[JetInfo[iJetColl].nJet]    = Bprob;
    JetInfo[iJetColl].Jet_SvxN[JetInfo[iJetColl].nJet]     = SvtxN;
    JetInfo[iJetColl].Jet_Svx[JetInfo[iJetColl].nJet]      = Svtx;
    JetInfo[iJetColl].Jet_SvxNHP[JetInfo[iJetColl].nJet]   = SvtxNHP;
    JetInfo[iJetColl].Jet_SvxHP[JetInfo[iJetColl].nJet]    = SvtxHP;
    JetInfo[iJetColl].Jet_CombSvxN[JetInfo[iJetColl].nJet] = CombinedSvtxN;
    JetInfo[iJetColl].Jet_CombSvxP[JetInfo[iJetColl].nJet] = CombinedSvtxP;
    JetInfo[iJetColl].Jet_CombSvx[JetInfo[iJetColl].nJet]  = CombinedSvtx;
    JetInfo[iJetColl].Jet_CombIVF[JetInfo[iJetColl].nJet]   = CombinedIVF;
    JetInfo[iJetColl].Jet_CombIVF_P[JetInfo[iJetColl].nJet] = CombinedIVF_P;
    JetInfo[iJetColl].Jet_CombIVF_N[JetInfo[iJetColl].nJet] = CombinedIVF_N;
    JetInfo[iJetColl].Jet_SoftMuN[JetInfo[iJetColl].nJet]  = SoftMN;
    JetInfo[iJetColl].Jet_SoftMuP[JetInfo[iJetColl].nJet]  = SoftMP;
    JetInfo[iJetColl].Jet_SoftMu[JetInfo[iJetColl].nJet]   = SoftM;
    JetInfo[iJetColl].Jet_SoftElN[JetInfo[iJetColl].nJet]  = SoftEN;
    JetInfo[iJetColl].Jet_SoftElP[JetInfo[iJetColl].nJet]  = SoftEP;
    JetInfo[iJetColl].Jet_SoftEl[JetInfo[iJetColl].nJet]   = SoftE;

    // TagInfo TaggingVariables
    if ( storeTagVariables_ )
    {
      reco::TaggingVariableList ipVars = ipTagInfo->taggingVariables();
      reco::TaggingVariableList svVars = svTagInfo->taggingVariables();
      int nTracks = ipTagInfo->selectedTracks().size();
      int nSVs = svTagInfo->nVertices();

      // per jet
      JetInfo[iJetColl].TagVar_jetNTracks[JetInfo[iJetColl].nJet]                  = nTracks;
      JetInfo[iJetColl].TagVar_jetNSecondaryVertices[JetInfo[iJetColl].nJet]       = nSVs;
      //-------------
      JetInfo[iJetColl].TagVar_chargedHadronEnergyFraction[JetInfo[iJetColl].nJet] = pjet->chargedHadronEnergyFraction();
      JetInfo[iJetColl].TagVar_neutralHadronEnergyFraction[JetInfo[iJetColl].nJet] = pjet->neutralHadronEnergyFraction();
      JetInfo[iJetColl].TagVar_photonEnergyFraction[JetInfo[iJetColl].nJet]        = pjet->photonEnergyFraction();
      JetInfo[iJetColl].TagVar_electronEnergyFraction[JetInfo[iJetColl].nJet]      = pjet->electronEnergyFraction();
      JetInfo[iJetColl].TagVar_muonEnergyFraction[JetInfo[iJetColl].nJet]          = pjet->muonEnergyFraction();
      JetInfo[iJetColl].TagVar_chargedHadronMultiplicity[JetInfo[iJetColl].nJet]   = pjet->chargedHadronMultiplicity();
      JetInfo[iJetColl].TagVar_neutralHadronMultiplicity[JetInfo[iJetColl].nJet]   = pjet->neutralHadronMultiplicity();
      JetInfo[iJetColl].TagVar_photonMultiplicity[JetInfo[iJetColl].nJet]          = pjet->photonMultiplicity();
      JetInfo[iJetColl].TagVar_electronMultiplicity[JetInfo[iJetColl].nJet]        = pjet->electronMultiplicity();
      JetInfo[iJetColl].TagVar_muonMultiplicity[JetInfo[iJetColl].nJet]            = pjet->muonMultiplicity();

      // per jet per track
      JetInfo[iJetColl].Jet_nFirstTrkTagVar[JetInfo[iJetColl].nJet] = JetInfo[iJetColl].nTrkTagVar;

      std::vector<float> tagValList = ipVars.getList(reco::btau::trackMomentum,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVar_trackMomentum[JetInfo[iJetColl].nTrkTagVar] );
      tagValList = ipVars.getList(reco::btau::trackEta,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVar_trackEta[JetInfo[iJetColl].nTrkTagVar] );
      tagValList = ipVars.getList(reco::btau::trackPhi,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVar_trackPhi[JetInfo[iJetColl].nTrkTagVar] );
      tagValList = ipVars.getList(reco::btau::trackPtRel,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVar_trackPtRel[JetInfo[iJetColl].nTrkTagVar] );
      tagValList = ipVars.getList(reco::btau::trackPPar,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVar_trackPPar[JetInfo[iJetColl].nTrkTagVar] );
      tagValList = ipVars.getList(reco::btau::trackEtaRel,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVar_trackEtaRel[JetInfo[iJetColl].nTrkTagVar] );
      tagValList = ipVars.getList(reco::btau::trackDeltaR,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVar_trackDeltaR[JetInfo[iJetColl].nTrkTagVar] );
      tagValList = ipVars.getList(reco::btau::trackPtRatio,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVar_trackPtRatio[JetInfo[iJetColl].nTrkTagVar] );
      tagValList = ipVars.getList(reco::btau::trackPParRatio,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVar_trackPParRatio[JetInfo[iJetColl].nTrkTagVar] );
      tagValList = ipVars.getList(reco::btau::trackSip2dVal,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVar_trackSip2dVal[JetInfo[iJetColl].nTrkTagVar] );
      tagValList = ipVars.getList(reco::btau::trackSip2dSig,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVar_trackSip2dSig[JetInfo[iJetColl].nTrkTagVar] );
      tagValList = ipVars.getList(reco::btau::trackSip3dVal,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVar_trackSip3dVal[JetInfo[iJetColl].nTrkTagVar] );
      tagValList = ipVars.getList(reco::btau::trackSip3dSig,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVar_trackSip3dSig[JetInfo[iJetColl].nTrkTagVar] );
      tagValList = ipVars.getList(reco::btau::trackDecayLenVal,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVar_trackDecayLenVal[JetInfo[iJetColl].nTrkTagVar] );
      tagValList = ipVars.getList(reco::btau::trackDecayLenSig,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVar_trackDecayLenSig[JetInfo[iJetColl].nTrkTagVar] );
      tagValList = ipVars.getList(reco::btau::trackJetDistVal,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVar_trackJetDistVal[JetInfo[iJetColl].nTrkTagVar] );
      tagValList = ipVars.getList(reco::btau::trackJetDistSig,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVar_trackJetDistSig[JetInfo[iJetColl].nTrkTagVar] );
      tagValList = ipVars.getList(reco::btau::trackChi2,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVar_trackChi2[JetInfo[iJetColl].nTrkTagVar] );
      tagValList = ipVars.getList(reco::btau::trackNTotalHits,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVar_trackNTotalHits[JetInfo[iJetColl].nTrkTagVar] );
      tagValList = ipVars.getList(reco::btau::trackNPixelHits,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVar_trackNPixelHits[JetInfo[iJetColl].nTrkTagVar] );

      JetInfo[iJetColl].nTrkTagVar += nTracks;
      JetInfo[iJetColl].Jet_nLastTrkTagVar[JetInfo[iJetColl].nJet] = JetInfo[iJetColl].nTrkTagVar;

      // per jet per secondary vertex
      JetInfo[iJetColl].Jet_nFirstSVTagVar[JetInfo[iJetColl].nJet] = JetInfo[iJetColl].nSVTagVar;

      for(int svIdx=0; svIdx < nSVs; ++svIdx)
      {
        JetInfo[iJetColl].TagVar_vertexMass[JetInfo[iJetColl].nSVTagVar + svIdx]    = svTagInfo->secondaryVertex(svIdx).p4().mass();
        //JetInfo[iJetColl].TagVar_vertexNTracks[JetInfo[iJetColl].nSVTagVar + svIdx] = svTagInfo->secondaryVertex(svIdx).nTracks();
      }
      tagValList = svVars.getList(reco::btau::vertexJetDeltaR,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVar_vertexJetDeltaR[JetInfo[iJetColl].nSVTagVar] );
      tagValList = svVars.getList(reco::btau::flightDistance2dVal,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVar_flightDistance2dVal[JetInfo[iJetColl].nSVTagVar] );
      tagValList = svVars.getList(reco::btau::flightDistance2dSig,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVar_flightDistance2dSig[JetInfo[iJetColl].nSVTagVar] );
      tagValList = svVars.getList(reco::btau::flightDistance3dVal,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVar_flightDistance3dVal[JetInfo[iJetColl].nSVTagVar] );
      tagValList = svVars.getList(reco::btau::flightDistance3dSig,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVar_flightDistance3dSig[JetInfo[iJetColl].nSVTagVar] );

      JetInfo[iJetColl].nSVTagVar += nSVs;
      JetInfo[iJetColl].Jet_nLastSVTagVar[JetInfo[iJetColl].nJet] = JetInfo[iJetColl].nSVTagVar;
    }

    // CSV TaggingVariables
    if ( storeCSVTagVariables_ )
    {
      std::vector<const reco::BaseTagInfo*>  baseTagInfos;
      JetTagComputer::TagInfoHelper helper(baseTagInfos);
      baseTagInfos.push_back( ipTagInfo );
      baseTagInfos.push_back( svTagInfo );
      // TaggingVariables
      reco::TaggingVariableList vars = computer->taggingVariables(helper);

      // per jet
      JetInfo[iJetColl].TagVarCSV_trackJetPt[JetInfo[iJetColl].nJet]                  = ( vars.checkTag(reco::btau::trackJetPt) ? vars.get(reco::btau::trackJetPt) : -9999 );
      JetInfo[iJetColl].TagVarCSV_vertexCategory[JetInfo[iJetColl].nJet]              = ( vars.checkTag(reco::btau::vertexCategory) ? vars.get(reco::btau::vertexCategory) : -9999 );
      JetInfo[iJetColl].TagVarCSV_jetNSecondaryVertices[JetInfo[iJetColl].nJet]       = ( vars.checkTag(reco::btau::jetNSecondaryVertices) ? vars.get(reco::btau::jetNSecondaryVertices) : 0 );
      JetInfo[iJetColl].TagVarCSV_trackSumJetEtRatio[JetInfo[iJetColl].nJet]          = ( vars.checkTag(reco::btau::trackSumJetEtRatio) ? vars.get(reco::btau::trackSumJetEtRatio) : -9999 );
      JetInfo[iJetColl].TagVarCSV_trackSumJetDeltaR[JetInfo[iJetColl].nJet]           = ( vars.checkTag(reco::btau::trackSumJetDeltaR) ? vars.get(reco::btau::trackSumJetDeltaR) : -9999 );
      JetInfo[iJetColl].TagVarCSV_trackSip2dValAboveCharm[JetInfo[iJetColl].nJet]     = ( vars.checkTag(reco::btau::trackSip2dValAboveCharm) ? vars.get(reco::btau::trackSip2dValAboveCharm) : -9999 );
      JetInfo[iJetColl].TagVarCSV_trackSip2dSigAboveCharm[JetInfo[iJetColl].nJet]     = ( vars.checkTag(reco::btau::trackSip2dSigAboveCharm) ? vars.get(reco::btau::trackSip2dSigAboveCharm) : -9999 );
      JetInfo[iJetColl].TagVarCSV_trackSip3dValAboveCharm[JetInfo[iJetColl].nJet]     = ( vars.checkTag(reco::btau::trackSip3dValAboveCharm) ? vars.get(reco::btau::trackSip3dValAboveCharm) : -9999 );
      JetInfo[iJetColl].TagVarCSV_trackSip3dSigAboveCharm[JetInfo[iJetColl].nJet]     = ( vars.checkTag(reco::btau::trackSip3dSigAboveCharm) ? vars.get(reco::btau::trackSip3dSigAboveCharm) : -9999 );
      JetInfo[iJetColl].TagVarCSV_vertexMass[JetInfo[iJetColl].nJet]                  = ( vars.checkTag(reco::btau::vertexMass) ? vars.get(reco::btau::vertexMass) : -9999 );
      JetInfo[iJetColl].TagVarCSV_vertexNTracks[JetInfo[iJetColl].nJet]               = ( vars.checkTag(reco::btau::vertexNTracks) ? vars.get(reco::btau::vertexNTracks) : 0 );
      JetInfo[iJetColl].TagVarCSV_vertexEnergyRatio[JetInfo[iJetColl].nJet]           = ( vars.checkTag(reco::btau::vertexEnergyRatio) ? vars.get(reco::btau::vertexEnergyRatio) : -9999 );
      JetInfo[iJetColl].TagVarCSV_vertexJetDeltaR[JetInfo[iJetColl].nJet]             = ( vars.checkTag(reco::btau::vertexJetDeltaR) ? vars.get(reco::btau::vertexJetDeltaR) : -9999 );
      JetInfo[iJetColl].TagVarCSV_flightDistance2dVal[JetInfo[iJetColl].nJet]         = ( vars.checkTag(reco::btau::flightDistance2dVal) ? vars.get(reco::btau::flightDistance2dVal) : -9999 );
      JetInfo[iJetColl].TagVarCSV_flightDistance2dSig[JetInfo[iJetColl].nJet]         = ( vars.checkTag(reco::btau::flightDistance2dSig) ? vars.get(reco::btau::flightDistance2dSig) : -9999 );
      JetInfo[iJetColl].TagVarCSV_flightDistance3dVal[JetInfo[iJetColl].nJet]         = ( vars.checkTag(reco::btau::flightDistance3dVal) ? vars.get(reco::btau::flightDistance3dVal) : -9999 );
      JetInfo[iJetColl].TagVarCSV_flightDistance3dSig[JetInfo[iJetColl].nJet]         = ( vars.checkTag(reco::btau::flightDistance3dSig) ? vars.get(reco::btau::flightDistance3dSig) : -9999 );

      // per jet per track
      JetInfo[iJetColl].Jet_nFirstTrkTagVarCSV[JetInfo[iJetColl].nJet] = JetInfo[iJetColl].nTrkTagVarCSV;
      std::vector<float> tagValList = vars.getList(reco::btau::trackSip2dSig,false);
      JetInfo[iJetColl].TagVarCSV_jetNTracks[JetInfo[iJetColl].nJet] = tagValList.size();

      tagValList = vars.getList(reco::btau::trackMomentum,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVarCSV_trackMomentum[JetInfo[iJetColl].nTrkTagVarCSV] );
      tagValList = vars.getList(reco::btau::trackEta,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVarCSV_trackEta[JetInfo[iJetColl].nTrkTagVarCSV] );
      tagValList = vars.getList(reco::btau::trackPhi,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVarCSV_trackPhi[JetInfo[iJetColl].nTrkTagVarCSV] );
      tagValList = vars.getList(reco::btau::trackPtRel,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVarCSV_trackPtRel[JetInfo[iJetColl].nTrkTagVarCSV] );
      tagValList = vars.getList(reco::btau::trackPPar,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVarCSV_trackPPar[JetInfo[iJetColl].nTrkTagVarCSV] );
      tagValList = vars.getList(reco::btau::trackDeltaR,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVarCSV_trackDeltaR[JetInfo[iJetColl].nTrkTagVarCSV] );
      tagValList = vars.getList(reco::btau::trackPtRatio,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVarCSV_trackPtRatio[JetInfo[iJetColl].nTrkTagVarCSV] );
      tagValList = vars.getList(reco::btau::trackPParRatio,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVarCSV_trackPParRatio[JetInfo[iJetColl].nTrkTagVarCSV] );
      tagValList = vars.getList(reco::btau::trackSip2dVal,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVarCSV_trackSip2dVal[JetInfo[iJetColl].nTrkTagVarCSV] );
      tagValList = vars.getList(reco::btau::trackSip2dSig,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVarCSV_trackSip2dSig[JetInfo[iJetColl].nTrkTagVarCSV] );
      tagValList = vars.getList(reco::btau::trackSip3dVal,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVarCSV_trackSip3dVal[JetInfo[iJetColl].nTrkTagVarCSV] );
      tagValList = vars.getList(reco::btau::trackSip3dSig,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVarCSV_trackSip3dSig[JetInfo[iJetColl].nTrkTagVarCSV] );
      tagValList = vars.getList(reco::btau::trackDecayLenVal,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVarCSV_trackDecayLenVal[JetInfo[iJetColl].nTrkTagVarCSV] );
      tagValList = vars.getList(reco::btau::trackDecayLenSig,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVarCSV_trackDecayLenSig[JetInfo[iJetColl].nTrkTagVarCSV] );
      tagValList = vars.getList(reco::btau::trackJetDistVal,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVarCSV_trackJetDistVal[JetInfo[iJetColl].nTrkTagVarCSV] );
      tagValList = vars.getList(reco::btau::trackJetDistSig,false);
      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVarCSV_trackJetDistSig[JetInfo[iJetColl].nTrkTagVarCSV] );

      JetInfo[iJetColl].nTrkTagVarCSV += JetInfo[iJetColl].TagVarCSV_jetNTracks[JetInfo[iJetColl].nJet];
      JetInfo[iJetColl].Jet_nLastTrkTagVarCSV[JetInfo[iJetColl].nJet] = JetInfo[iJetColl].nTrkTagVarCSV;
      //---------------------------
      JetInfo[iJetColl].Jet_nFirstTrkEtaRelTagVarCSV[JetInfo[iJetColl].nJet] = JetInfo[iJetColl].nTrkEtaRelTagVarCSV;
      tagValList = vars.getList(reco::btau::trackEtaRel,false);
      JetInfo[iJetColl].TagVarCSV_jetNTracksEtaRel[JetInfo[iJetColl].nJet] = tagValList.size();

      if(tagValList.size()>0) std::copy( tagValList.begin(), tagValList.end(), &JetInfo[iJetColl].TagVarCSV_trackEtaRel[JetInfo[iJetColl].nTrkEtaRelTagVarCSV] );

      JetInfo[iJetColl].nTrkEtaRelTagVarCSV += JetInfo[iJetColl].TagVarCSV_jetNTracksEtaRel[JetInfo[iJetColl].nJet];
      JetInfo[iJetColl].Jet_nLastTrkEtaRelTagVarCSV[JetInfo[iJetColl].nJet] = JetInfo[iJetColl].nTrkEtaRelTagVarCSV;
    }

    //*****************************************************************
    // get track histories associated to sec. vertex (for simple SV)
    //*****************************************************************
    JetInfo[iJetColl].Jet_nFirstSV[JetInfo[iJetColl].nJet]  = JetInfo[iJetColl].nSV;
    JetInfo[iJetColl].Jet_SV_multi[JetInfo[iJetColl].nJet]  = svTagInfo->nVertices();

    // if secondary vertices present
    for (int vtx = 0; vtx < JetInfo[iJetColl].Jet_SV_multi[JetInfo[iJetColl].nJet]; ++vtx )
    {

      JetInfo[iJetColl].SV_x[JetInfo[iJetColl].nSV]    = position(svTagInfo->secondaryVertex(vtx)).x();
      JetInfo[iJetColl].SV_y[JetInfo[iJetColl].nSV]    = position(svTagInfo->secondaryVertex(vtx)).y();
      JetInfo[iJetColl].SV_z[JetInfo[iJetColl].nSV]    = position(svTagInfo->secondaryVertex(vtx)).z();
      JetInfo[iJetColl].SV_ex[JetInfo[iJetColl].nSV]   = xError(svTagInfo->secondaryVertex(vtx));
      JetInfo[iJetColl].SV_ey[JetInfo[iJetColl].nSV]   = yError(svTagInfo->secondaryVertex(vtx));
      JetInfo[iJetColl].SV_ez[JetInfo[iJetColl].nSV]   = zError(svTagInfo->secondaryVertex(vtx));
      JetInfo[iJetColl].SV_chi2[JetInfo[iJetColl].nSV] = chi2(svTagInfo->secondaryVertex(vtx));
      JetInfo[iJetColl].SV_ndf[JetInfo[iJetColl].nSV]  = ndof(svTagInfo->secondaryVertex(vtx));

      JetInfo[iJetColl].SV_flight[JetInfo[iJetColl].nSV]      = svTagInfo->flightDistance(vtx).value();
      JetInfo[iJetColl].SV_flightErr[JetInfo[iJetColl].nSV]   = svTagInfo->flightDistance(vtx).error();
      JetInfo[iJetColl].SV_flight2D[JetInfo[iJetColl].nSV]    = svTagInfo->flightDistance(vtx, true).value();
      JetInfo[iJetColl].SV_flight2DErr[JetInfo[iJetColl].nSV] = svTagInfo->flightDistance(vtx, true).error();
      JetInfo[iJetColl].SV_nTrk[JetInfo[iJetColl].nSV]        = nTracks(svTagInfo->secondaryVertex(vtx));


      const Vertex &vertex = svTagInfo->secondaryVertex(vtx);

      JetInfo[iJetColl].SV_vtx_pt[JetInfo[iJetColl].nSV]  = vertex.p4().pt();
      JetInfo[iJetColl].SV_vtx_eta[JetInfo[iJetColl].nSV] = vertex.p4().eta();
      JetInfo[iJetColl].SV_vtx_phi[JetInfo[iJetColl].nSV] = vertex.p4().phi();
      JetInfo[iJetColl].SV_mass[JetInfo[iJetColl].nSV]    = vertex.p4().mass();

      Int_t totcharge=0;
      reco::TrackKinematics vertexKinematics;

      // get the vertex kinematics and charge
      vertexKinematicsAndChange(vertex, vertexKinematics, totcharge);

      // total charge at the secondary vertex
      JetInfo[iJetColl].SV_totCharge[JetInfo[iJetColl].nSV]=totcharge;
	
	

      math::XYZTLorentzVector vertexSum = vertexKinematics.weightedVectorSum();
      edm::RefToBase<reco::Jet> jet = ipTagInfo->jet();
      math::XYZVector jetDir = jet->momentum().Unit();
      GlobalVector flightDir = svTagInfo->flightDirection(vtx);

      JetInfo[iJetColl].SV_deltaR_jet[JetInfo[iJetColl].nSV]     = ( reco::deltaR(flightDir, jetDir) );
      JetInfo[iJetColl].SV_deltaR_sum_jet[JetInfo[iJetColl].nSV] = ( reco::deltaR(vertexSum, jetDir) );
      JetInfo[iJetColl].SV_deltaR_sum_dir[JetInfo[iJetColl].nSV] = ( reco::deltaR(vertexSum, flightDir) );

      Line::PositionType pos(GlobalPoint(position(vertex).x(),position(vertex).y(),position(vertex).z()));
      Line trackline(pos,flightDir);
      // get the Jet  line
      Line::PositionType pos2(GlobalPoint(pv->x(),pv->y(),pv->z()));
      Line::DirectionType dir2(GlobalVector(jetDir.x(),jetDir.y(),jetDir.z()));
      Line jetline(pos2,dir2);
      // now compute the distance between the two lines
      JetInfo[iJetColl].SV_vtxDistJetAxis[JetInfo[iJetColl].nSV] = (jetline.distance(trackline)).mag();


      math::XYZTLorentzVector allSum =  allKinematics.weightedVectorSum() ; //allKinematics.vectorSum()
      JetInfo[iJetColl].SV_EnergyRatio[JetInfo[iJetColl].nSV]= vertexSum.E() / allSum.E();
	

      JetInfo[iJetColl].SV_dir_x[JetInfo[iJetColl].nSV]= flightDir.x();
      JetInfo[iJetColl].SV_dir_y[JetInfo[iJetColl].nSV]= flightDir.y();		
      JetInfo[iJetColl].SV_dir_z[JetInfo[iJetColl].nSV]= flightDir.z(); 




      ++JetInfo[iJetColl].nSV;

    } //// if secondary vertices present
    JetInfo[iJetColl].Jet_nLastSV[JetInfo[iJetColl].nJet] = JetInfo[iJetColl].nSV;

    ++JetInfo[iJetColl].nJet;
  } // end loop on jet

  return;
} // BTagAnalyzerLiteT:: processJets


template<typename IPTI,typename VTX>
void BTagAnalyzerLiteT<IPTI,VTX>::setTracksPVBase(const reco::TrackRef & trackRef, const edm::Handle<reco::VertexCollection> & pvHandle, int & iPV, float & PVweight)
{
  iPV = -1;
  PVweight = 0.;

  const reco::TrackBaseRef trackBaseRef( trackRef );

  typedef reco::VertexCollection::const_iterator IV;
  typedef reco::Vertex::trackRef_iterator IT;

  for(IV iv=pvHandle->begin(); iv!=pvHandle->end(); ++iv)
  {
    const reco::Vertex & vtx = *iv;
    // loop over tracks in vertices
    for(IT it=vtx.tracks_begin(); it!=vtx.tracks_end(); ++it)
    {
      const reco::TrackBaseRef & baseRef = *it;
      // one of the tracks in the vertex is the same as the track considered in the function
      if( baseRef == trackBaseRef )
      {
        float w = vtx.trackWeight(baseRef);
        // select the vertex for which the track has the highest weight
        if( w > PVweight )
        {
          PVweight = w;
          iPV = ( iv - pvHandle->begin() );
          break;
        }
      }
    }
  }
}


// ------------ method called once each job just before starting event loop  ------------
template<typename IPTI,typename VTX>
void BTagAnalyzerLiteT<IPTI,VTX>::beginJob() {
}


// ------------ method called once each job just after ending the event loop  ------------
template<typename IPTI,typename VTX>
void BTagAnalyzerLiteT<IPTI,VTX>::endJob() {
}


template<typename IPTI,typename VTX>
bool BTagAnalyzerLiteT<IPTI,VTX>::isHardProcess(const int status)
{
  // if Pythia8
  if( hadronizerType_ & (1 << 1) )
  {
    if( status>=21 && status<=29 )
      return true;
  }
  else // assuming Pythia6
  {
    if( status==3 )
      return true;
  }

  return false;
}

// -------------------------------------------------------------------------
// NameCompatible
// -------------------------------------------------------------------------
template<typename IPTI,typename VTX>
bool BTagAnalyzerLiteT<IPTI,VTX>::NameCompatible(const std::string& pattern, const std::string& name)
{
  const boost::regex regexp(edm::glob2reg(pattern));

  return boost::regex_match(name,regexp);
}

// ------------ method that matches groomed and original jets based on minimum dR ------------
template<typename IPTI,typename VTX>
void BTagAnalyzerLiteT<IPTI,VTX>::matchGroomedJets(const edm::Handle<PatJetCollection>& jets,
                                                   const edm::Handle<PatJetCollection>& groomedJets,
                                                   std::vector<int>& matchedIndices)
{
   std::vector<bool> jetLocks(jets->size(),false);
   std::vector<int>  jetIndices;

   for(size_t gj=0; gj<groomedJets->size(); ++gj)
   {
     double matchedDR2 = 1e9;
     int matchedIdx = -1;

     for(size_t j=0; j<jets->size(); ++j)
     {
       if( jetLocks.at(j) ) continue; // skip jets that have already been matched

       double tempDR2 = reco::deltaR2( jets->at(j).rapidity(), jets->at(j).phi(), groomedJets->at(gj).rapidity(), groomedJets->at(gj).phi() );
       if( tempDR2 < matchedDR2 )
       {
         matchedDR2 = tempDR2;
         matchedIdx = j;
       }
     }

     if( matchedIdx>=0 ) jetLocks.at(matchedIdx) = true;
     jetIndices.push_back(matchedIdx);
   }

   if( std::find( jetIndices.begin(), jetIndices.end(), -1 ) != jetIndices.end() )
     edm::LogError("JetMatchingFailed") << "Matching groomed to original jets failed. Please check that the two jet collections belong to each other.";

   for(size_t j=0; j<jets->size(); ++j)
   {
     std::vector<int>::iterator matchedIndex = std::find( jetIndices.begin(), jetIndices.end(), j );

     matchedIndices.push_back( matchedIndex != jetIndices.end() ? std::distance(jetIndices.begin(),matchedIndex) : -1 );
   }
}

// -------------- template specializations --------------------
// -------------- toIPTagInfo ----------------
template<>
const BTagAnalyzerLiteT<reco::TrackIPTagInfo,reco::Vertex>::IPTagInfo *
BTagAnalyzerLiteT<reco::TrackIPTagInfo,reco::Vertex>::toIPTagInfo(const pat::Jet & jet, const std::string & tagInfos)
{
  return jet.tagInfoTrackIP(tagInfos.c_str());
}

template<>
const BTagAnalyzerLiteT<reco::CandIPTagInfo,reco::VertexCompositePtrCandidate>::IPTagInfo *
BTagAnalyzerLiteT<reco::CandIPTagInfo,reco::VertexCompositePtrCandidate>::toIPTagInfo(const pat::Jet & jet, const std::string & tagInfos)
{
  return jet.tagInfoCandIP(tagInfos.c_str());
}

// -------------- toSVTagInfo ----------------
template<>
const BTagAnalyzerLiteT<reco::TrackIPTagInfo,reco::Vertex>::SVTagInfo *
BTagAnalyzerLiteT<reco::TrackIPTagInfo,reco::Vertex>::toSVTagInfo(const pat::Jet & jet, const std::string & tagInfos)
{
  return jet.tagInfoSecondaryVertex(tagInfos.c_str());
}

template<>
const BTagAnalyzerLiteT<reco::CandIPTagInfo,reco::VertexCompositePtrCandidate>::SVTagInfo *
BTagAnalyzerLiteT<reco::CandIPTagInfo,reco::VertexCompositePtrCandidate>::toSVTagInfo(const pat::Jet & jet, const std::string & tagInfos)
{
  return jet.tagInfoCandSecondaryVertex(tagInfos.c_str());
}

// -------------- setTracksPV ----------------
template<>
void BTagAnalyzerLiteT<reco::TrackIPTagInfo,reco::Vertex>::setTracksPV(const TrackRef & trackRef, const edm::Handle<reco::VertexCollection> & pvHandle, int & iPV, float & PVweight)
{
  setTracksPVBase(trackRef, pvHandle, iPV, PVweight);
}

template<>
void BTagAnalyzerLiteT<reco::CandIPTagInfo,reco::VertexCompositePtrCandidate>::setTracksPV(const TrackRef & trackRef, const edm::Handle<reco::VertexCollection> & pvHandle, int & iPV, float & PVweight)
{
  iPV = -1;
  PVweight = 0.;

  const pat::PackedCandidate * pcand = dynamic_cast<const pat::PackedCandidate *>(trackRef.get());

  if(pcand) // MiniAOD case
  {
    if( pcand->fromPV() == pat::PackedCandidate::PVUsedInFit )
    {
      iPV = 0;
      PVweight = 1.;
    }
  }
  else
  {
    const reco::PFCandidate * pfcand = dynamic_cast<const reco::PFCandidate *>(trackRef.get());

    setTracksPVBase(pfcand->trackRef(), pvHandle, iPV, PVweight);
  }
}

// -------------- setTracksSV ----------------
template<>
void BTagAnalyzerLiteT<reco::TrackIPTagInfo,reco::Vertex>::setTracksSV(const TrackRef & trackRef, const SVTagInfo * svTagInfo, int & isFromSV, int & iSV, float & SVweight)
{
  isFromSV = 0;
  iSV = -1;
  SVweight = 0.;

  const reco::TrackBaseRef trackBaseRef( trackRef );

  typedef reco::Vertex::trackRef_iterator IT;

  size_t nSV = svTagInfo->nVertices();
  for(size_t iv=0; iv<nSV; ++iv)
  {
    const reco::Vertex & vtx = svTagInfo->secondaryVertex(iv);
    // loop over tracks in vertices
    for(IT it=vtx.tracks_begin(); it!=vtx.tracks_end(); ++it)
    {
      const reco::TrackBaseRef & baseRef = *it;
      // one of the tracks in the vertex is the same as the track considered in the function
      if( baseRef == trackBaseRef )
      {
        float w = vtx.trackWeight(baseRef);
        // select the vertex for which the track has the highest weight
        if( w > SVweight )
        {
          SVweight = w;
          isFromSV = 1;
          iSV = iv;
          break;
        }
      }
    }
  }
}

template<>
void BTagAnalyzerLiteT<reco::CandIPTagInfo,reco::VertexCompositePtrCandidate>::setTracksSV(const TrackRef & trackRef, const SVTagInfo * svTagInfo, int & isFromSV, int & iSV, float & SVweight)
{
  isFromSV = 0;
  iSV = -1;
  SVweight = 0.;

  typedef std::vector<reco::CandidatePtr>::const_iterator IT;

  size_t nSV = svTagInfo->nVertices();
  for(size_t iv=0; iv<nSV; ++iv)
  {
    const Vertex & vtx = svTagInfo->secondaryVertex(iv);
    const std::vector<reco::CandidatePtr> & tracks = vtx.daughterPtrVector();

    // one of the tracks in the vertex is the same as the track considered in the function
    if( std::find(tracks.begin(),tracks.end(),trackRef) != tracks.end() )
    {
      SVweight = 1.;
      isFromSV = 1;
      iSV = iv;
    }

    // select the first vertex for which the track is used in the fit
    // (reco::VertexCompositePtrCandidate does not store track weights so can't select the vertex for which the track has the highest weight)
    if(iSV>=0)
      break;
  }
}

// -------------- vertexKinematicsAndChange ----------------
template<>
void BTagAnalyzerLiteT<reco::TrackIPTagInfo,reco::Vertex>::vertexKinematicsAndChange(const Vertex & vertex, reco::TrackKinematics & vertexKinematics, Int_t & charge)
{
  Bool_t hasRefittedTracks = vertex.hasRefittedTracks();

  for(reco::Vertex::trackRef_iterator track = vertex.tracks_begin();
      track != vertex.tracks_end(); ++track) {
    Double_t w = vertex.trackWeight(*track);
    if (w < 0.5)
      continue;
    if (hasRefittedTracks) {
      reco::Track actualTrack = vertex.refittedTrack(*track);
      vertexKinematics.add(actualTrack, w);
      charge+=actualTrack.charge();
    }
    else {
      const reco::Track& mytrack = **track;
      vertexKinematics.add(mytrack, w);
      charge+=mytrack.charge();
    }
  }
}

template<>
void BTagAnalyzerLiteT<reco::CandIPTagInfo,reco::VertexCompositePtrCandidate>::vertexKinematicsAndChange(const Vertex & vertex, reco::TrackKinematics & vertexKinematics, Int_t & charge)
{
  const std::vector<reco::CandidatePtr> & tracks = vertex.daughterPtrVector();

  for(std::vector<reco::CandidatePtr>::const_iterator track = tracks.begin(); track != tracks.end(); ++track) {
    const reco::Track& mytrack = *(*track)->bestTrack();
    vertexKinematics.add(mytrack, 1.0);
    charge+=mytrack.charge();
  }
}

// -------------- recalcNsubjettiness ----------------
template<>
void BTagAnalyzerLiteT<reco::TrackIPTagInfo,reco::Vertex>::recalcNsubjettiness(const pat::Jet & jet, const SVTagInfo & svTagInfo, float & tau1, float & tau2)
{
  // need candidate-based IVF vertices so do nothing here
}

template<>
void BTagAnalyzerLiteT<reco::CandIPTagInfo,reco::VertexCompositePtrCandidate>::recalcNsubjettiness(const pat::Jet & jet, const SVTagInfo & svTagInfo, float & tau1, float & tau2)
{
  std::vector<fastjet::PseudoJet> fjParticles;
  std::vector<reco::CandidatePtr> svDaughters;

  // loop over IVF vertices and push them in the vector of FastJet constituents and also collect their daughters
  for(size_t i=0; i<svTagInfo.nVertices(); ++i)
  {
    const reco::VertexCompositePtrCandidate & vtx = svTagInfo.secondaryVertex(i);

    fjParticles.push_back( fastjet::PseudoJet( vtx.px(), vtx.py(), vtx.pz(), vtx.energy() ) );

    const std::vector<reco::CandidatePtr> & daughters = vtx.daughterPtrVector();
    svDaughters.insert(svDaughters.end(), daughters.begin(), daughters.end());
  }

  // loop over jet constituents and select those that are not daughters of IVF vertices
  std::vector<reco::CandidatePtr> constituentsOther;
  for(const reco::CandidatePtr & daughter : jet.daughterPtrVector())
  {
    if (std::find(svDaughters.begin(), svDaughters.end(), daughter) == svDaughters.end())
      constituentsOther.push_back( daughter );
  }

  // loop over jet constituents that are not daughters of IVF vertices and push them in the vector of FastJet constituents
  for(const reco::CandidatePtr & constit : constituentsOther)
  {
    if ( constit.isNonnull() && constit.isAvailable() )
      fjParticles.push_back( fastjet::PseudoJet( constit->px(), constit->py(), constit->pz(), constit->energy() ) );
    else
      edm::LogWarning("MissingJetConstituent") << "Jet constituent required for N-subjettiness computation is missing!";
  }

  // re-calculate N-subjettiness
  tau1 = njettiness_.getTau(1, fjParticles);
  tau2 = njettiness_.getTau(2, fjParticles);
}


// define specific instances of the templated BTagAnalyzerLite
typedef BTagAnalyzerLiteT<reco::TrackIPTagInfo,reco::Vertex> BTagAnalyzerLiteLegacy;
typedef BTagAnalyzerLiteT<reco::CandIPTagInfo,reco::VertexCompositePtrCandidate> BTagAnalyzerLite;

//define plugins
DEFINE_FWK_MODULE(BTagAnalyzerLiteLegacy);
DEFINE_FWK_MODULE(BTagAnalyzerLite);
