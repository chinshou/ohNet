#include <OpenHome/Net/Private/CpiDeviceUpnp.h>
#include <OpenHome/Net/Private/CpiDevice.h>
#include <OpenHome/Private/Thread.h>
#include <OpenHome/Net/Private/Stack.h>
#include <OpenHome/Net/Private/CpiService.h>
#include <OpenHome/Private/Stream.h>
#include <OpenHome/Net/Core/OhNet.h>
#include <OpenHome/Private/Http.h>
#include <OpenHome/Private/Ascii.h>
#include <OpenHome/Private/Parser.h>
#include <OpenHome/Net/Private/XmlFetcher.h>
#include <OpenHome/Net/Private/ProtocolUpnp.h>
#include <OpenHome/Net/Private/XmlParser.h>
#include <OpenHome/Private/Converter.h>
#include <OpenHome/Private/Debug.h>
#include <OpenHome/Net/Private/DeviceXml.h>
#include <OpenHome/Net/Private/CpiSubscription.h>

#include <string.h>

using namespace OpenHome;
using namespace OpenHome::Net;


// CpiDeviceUpnp

CpiDeviceUpnp::CpiDeviceUpnp(const Brx& aUdn, const Brx& aLocation, TUint aMaxAgeSecs, IDeviceRemover& aDeviceList, CpiDeviceListUpnp& aList)
    : iLock("CDUP")
    , iLocation(aLocation)
    , iXmlFetch(NULL)
    , iDeviceXmlDocument(NULL)
    , iDeviceXml(NULL)
    , iExpiryTime(0)
    , iDeviceList(aDeviceList)
    , iList(&aList)
    , iSemReady("CDUS", 0)
    , iRemoved(false)
{
    iDevice = new CpiDevice(aUdn, *this, *this, this);
    iTimer = new Timer(MakeFunctor(*this, &CpiDeviceUpnp::TimerExpired));
    UpdateMaxAge(aMaxAgeSecs);
    iInvocable = new Invocable(*this);
}

const Brx& CpiDeviceUpnp::Udn() const
{
    return iDevice->Udn();
}

const Brx& CpiDeviceUpnp::Location() const
{
    return iLocation;
}

CpiDevice& CpiDeviceUpnp::Device()
{
    return *iDevice;
}

void CpiDeviceUpnp::UpdateMaxAge(TUint aSeconds)
{
    TUint delayMs = aSeconds * 1000;
    delayMs += 100; /* allow slightly longer than maxAge to cope with devices which
                       send out Alive messages at the last possible moment */
    TUint expiryTime = Os::TimeInMs() + delayMs;
    if (expiryTime >= iExpiryTime) {
        iExpiryTime = expiryTime;
        iTimer->FireAt(iExpiryTime);
    }
}

void CpiDeviceUpnp::FetchXml()
{
    AutoMutex a(iLock);
    iXmlFetch = XmlFetchManager::Fetch();
    Uri* uri = new Uri(iLocation);
    iDevice->AddRef();
    FunctorAsync functor = MakeFunctorAsync(*this, &CpiDeviceUpnp::XmlFetchCompleted);
    iXmlFetch->Set(uri, functor);
    XmlFetchManager::Fetch(iXmlFetch);
}

void CpiDeviceUpnp::InterruptXmlFetch()
{
    AutoMutex a(iLock);
    if (iXmlFetch != NULL) {
        iXmlFetch->Interrupt();
    }
    iList = NULL;
}

TBool CpiDeviceUpnp::GetAttribute(const char* aKey, Brh& aValue) const
{
    Brn key(aKey);
    
    Parser parser(key);
    
    if (parser.Next('.') == Brn("Upnp")) {
        Brn property = parser.Remaining();

        if (property == Brn("Location")) {
            aValue.Set(iLocation);
            return (true);
        }
        if (property == Brn("DeviceXml")) {
            aValue.Set(iXml);
            return (true);
        }

        const DeviceXml* device = iDeviceXml;
        
        if (parser.Next('.') == Brn("Root")) {
            device = &iDeviceXmlDocument->Root();
            property.Set(parser.Remaining());
        }
        
        try {
            if (property == Brn("FriendlyName")) {
                device->GetFriendlyName(aValue);
                return (true);
            }
            if (property == Brn("PresentationUrl")) {
                device->GetPresentationUrl(aValue);
                return (true);
            }
            
            Parser parser(property);
            
            Brn token = parser.Next('.');
            
            if (token == Brn("Service")) {
                aValue.Set(device->ServiceVersion(parser.Remaining()));
                return (true);
            }
        }
        catch (XmlError) {
        }
    }

    return (false);
}

void CpiDeviceUpnp::InvokeAction(Invocation& aInvocation)
{
    aInvocation.SetInvoker(*iInvocable);
    InvocationManager::Invoke(&aInvocation);
}

TUint CpiDeviceUpnp::Subscribe(CpiSubscription& aSubscription, const Uri& aSubscriber)
{
    TUint durationSecs = Stack::InitParams().SubscriptionDurationSecs();
    Uri uri;
    GetServiceUri(uri, "eventSubURL", aSubscription.ServiceType());
    EventUpnp eventUpnp(aSubscription);
    eventUpnp.Subscribe(uri, aSubscriber, durationSecs);
    return durationSecs;
}

TUint CpiDeviceUpnp::Renew(CpiSubscription& aSubscription)
{
    TUint durationSecs = Stack::InitParams().SubscriptionDurationSecs();
    Uri uri;
    GetServiceUri(uri, "eventSubURL", aSubscription.ServiceType());
    EventUpnp eventUpnp(aSubscription);
    eventUpnp.RenewSubscription(uri, durationSecs);
    return durationSecs;
}

void CpiDeviceUpnp::Unsubscribe(CpiSubscription& aSubscription, const Brx& aSid)
{
    Uri uri;
    GetServiceUri(uri, "eventSubURL", aSubscription.ServiceType());
    EventUpnp eventUpnp(aSubscription);
    eventUpnp.Unsubscribe(uri, aSid);
}

void CpiDeviceUpnp::NotifyRemovedBeforeReady()
{
    iLock.Wait();
    iRemoved = true;
    iLock.Signal();
    InterruptXmlFetch();
    iSemReady.Wait();
}

void CpiDeviceUpnp::Release()
{
    delete this; // iDevice not deleted here; it'll delete itself when this returns
}

CpiDeviceUpnp::~CpiDeviceUpnp()
{
    delete iDeviceXmlDocument;
    delete iDeviceXml;
    delete iTimer;
    delete iInvocable;
}

void CpiDeviceUpnp::TimerExpired()
{
    iDevice->SetExpired(true);
    iDeviceList.Remove(Udn());
}

void CpiDeviceUpnp::GetServiceUri(Uri& aUri, const TChar* aType, const ServiceType& aServiceType)
{
    Brn root = XmlParserBasic::Find("root", iXml);
    Brn device = XmlParserBasic::Find("device", root);
    Brn udn = XmlParserBasic::Find("UDN", device);
    if (!CpiDeviceUpnp::UdnMatches(udn, Udn())) {
        Brn deviceList = XmlParserBasic::Find("deviceList", device);
        do {
            Brn remaining;
            device.Set(XmlParserBasic::Find("device", deviceList, remaining));
            udn.Set(XmlParserBasic::Find("UDN", device));
            deviceList.Set(remaining);
        } while (!CpiDeviceUpnp::UdnMatches(udn, Udn()));
    }
    Brn serviceList = XmlParserBasic::Find("serviceList", device);
    Brn service;
    Brn serviceType;
    const Brx& targServiceType = aServiceType.FullName();
    do {
        Brn remaining;
        service.Set(XmlParserBasic::Find("service", serviceList, remaining));
        serviceType.Set(XmlParserBasic::Find("serviceType", service));
        serviceList.Set(remaining);
    } while (serviceType != targServiceType);
    Brn path = XmlParserBasic::Find(aType, service);

    // now create a uri using the scheme/host/port of the device xml location
    // plus the path we've just constructed
    Bws<40> base; // just need space for http://xxx.xxx.xxx.xxx:xxxxx
    aUri.Replace(iLocation);
    base.Append(aUri.Scheme());
    base.Append("://");
    base.Append(aUri.Host());
    base.Append(':');
    Ascii::AppendDec(base, aUri.Port());
    aUri.Replace(base, path);
}

TBool CpiDeviceUpnp::UdnMatches(const Brx& aFound, const Brx& aTarget)
{
    const Brn udnPrefix("uuid:");
    Brn udn(aFound);
    if (udn.Bytes() <= udnPrefix.Bytes()) {
        THROW(XmlError);
    }
    Brn prefix = udn.Split(0, udnPrefix.Bytes());
    if (prefix != udnPrefix) {
        THROW(XmlError);
    }
    udn.Set(udn.Split(udnPrefix.Bytes(), udn.Bytes() - udnPrefix.Bytes()));
    return (udn == aTarget);
}

void CpiDeviceUpnp::XmlFetchCompleted(IAsync& aAsync)
{
    iLock.Wait();
    iXmlFetch = NULL;
    iLock.Signal();
    TBool err = iRemoved;
    if (!err) {
        try {
            XmlFetch::Xml(aAsync).TransferTo(iXml);
        }
        catch (XmlFetchError&) {
            err = true;
            LOG2(kDevice, kError, "Error fetching xml for ");
            LOG2(kDevice, kError, Udn());
            LOG2(kDevice, kError, " from ");
            LOG2(kDevice, kError, iLocation);
            LOG2(kDevice, kError, "\n");
        }
    }
    if (!err) {
        try {
            iDeviceXmlDocument = new DeviceXmlDocument(iXml);
            iDeviceXml = new DeviceXml(iDeviceXmlDocument->Find(Udn()));
        }
        catch (XmlError&) {
            err = true;
            LOG2(kDevice, kError, "Error within xml for ");
            LOG2(kDevice, kError, Udn());
            LOG2(kDevice, kError, " from ");
            LOG2(kDevice, kError, iLocation);
            LOG2(kDevice, kError, ".  Xml is ");
            LOG2(kDevice, kError, iXml);
            LOG2(kDevice, kError, "\n");
        }
    }
    iLock.Wait();
    if (iList != NULL) {
        iList->XmlFetchCompleted(*this, err);
        iList = NULL;
    }
    iLock.Signal();
    iSemReady.Signal();
    iDevice->RemoveRef();
    // Don't add code after the RemoveRef(), we might have
    // just deleted this object!
}


// class CpiDeviceUpnp::Invocable

CpiDeviceUpnp::Invocable::Invocable(CpiDeviceUpnp& aDevice)
    : iDevice(aDevice)
{
}

void CpiDeviceUpnp::Invocable::InvokeAction(Invocation& aInvocation)
{
    try {
        Uri uri;
        iDevice.GetServiceUri(uri, "controlURL", aInvocation.ServiceType());
        InvocationUpnp invoker(aInvocation);
        invoker.Invoke(uri);
    }
    catch (XmlError&) {
        THROW(ReaderError);
    }
}


// CpiDeviceListUpnp

CpiDeviceListUpnp::CpiDeviceListUpnp(FunctorCpiDevice aAdded, FunctorCpiDevice aRemoved)
    : CpiDeviceList(aAdded, aRemoved)
    , iSsdpLock("DLSM")
    , iStarted(false)
{
    NetworkAdapterList& ifList = Stack::NetworkAdapterList();
    AutoNetworkAdapterRef ref("CpiDeviceListUpnp ctor");
    const NetworkAdapter* current = ref.Adapter();
    iRefreshTimer = new Timer(MakeFunctor(*this, &CpiDeviceListUpnp::RefreshTimerComplete));
    iNextRefreshTimer = new Timer(MakeFunctor(*this, &CpiDeviceListUpnp::NextRefreshDue));
    iPendingRefreshCount = 0;
    iInterfaceChangeListenerId = ifList.AddCurrentChangeListener(MakeFunctor(*this, &CpiDeviceListUpnp::CurrentNetworkAdapterChanged));
    iSubnetListChangeListenerId = ifList.AddSubnetListChangeListener(MakeFunctor(*this, &CpiDeviceListUpnp::SubnetListChanged));
    iSsdpLock.Wait();
    if (current == NULL) {
        iInterface = 0;
        iUnicastListener = NULL;
        iMulticastListener = NULL;
        iNotifyHandlerId = 0;
    }
    else {
        iInterface = current->Address();
        iUnicastListener = new SsdpListenerUnicast(*this, iInterface);
        iMulticastListener = &Stack::MulticastListenerClaim(iInterface);
        iNotifyHandlerId = iMulticastListener->AddNotifyHandler(this);
    }
    iSsdpLock.Signal();
}

CpiDeviceListUpnp::~CpiDeviceListUpnp()
{
    iLock.Wait();
    iActive = false;
    iLock.Signal();
    Stack::NetworkAdapterList().RemoveCurrentChangeListener(iInterfaceChangeListenerId);
    Stack::NetworkAdapterList().RemoveSubnetListChangeListener(iSubnetListChangeListenerId);
    iLock.Wait();
    Map::iterator it = iMap.begin();
    while (it != iMap.end()) {
        reinterpret_cast<CpiDeviceUpnp*>(it->second->OwnerData())->InterruptXmlFetch();
        it++;
    }
    for (TUint i=0; i<(TUint)iPendingRemove.size(); i++) {
        reinterpret_cast<CpiDeviceUpnp*>(iPendingRemove[i]->OwnerData())->InterruptXmlFetch();
    }
    iLock.Signal();
    delete iRefreshTimer;
    delete iNextRefreshTimer;
}

void CpiDeviceListUpnp::StopListeners()
{
    iSsdpLock.Wait();
    delete iUnicastListener;
    iUnicastListener = NULL;
    if (iMulticastListener != NULL) {
        iMulticastListener->RemoveNotifyHandler(iNotifyHandlerId);
        iNotifyHandlerId = 0;
        Stack::MulticastListenerRelease(iInterface);
        iMulticastListener = NULL;
    }
    iSsdpLock.Signal();
}

TBool CpiDeviceListUpnp::Update(const Brx& aUdn, const Brx& aLocation, TUint aMaxAge)
{
    if (!IsLocationReachable(aLocation)) {
        return false;
    }
    iLock.Wait();
    Stack::Mutex().Wait();
    if (iRefreshing && iPendingRefreshCount > 1) {
        // we need at most one final msearch once a network card starts working following an adapter change
        iPendingRefreshCount = 1;
    }
    Stack::Mutex().Signal();
    CpiDevice* device = RefDeviceLocked(aUdn);
    if (device != NULL) {
        CpiDeviceUpnp* deviceUpnp = reinterpret_cast<CpiDeviceUpnp*>(device->OwnerData());
        if (deviceUpnp->Location() != aLocation) {
            // device appears to have moved to a new location.
            // Remove the old record, leaving the caller to add the new one.
            iLock.Signal();
            Remove(aUdn);
            device->RemoveRef();
            return false;
        }
        deviceUpnp->UpdateMaxAge(aMaxAge);
        device->RemoveRef();
        iLock.Signal();
        LOG(kTrace, "Device alive {udn{");
        LOG(kTrace, aUdn);
        LOG(kTrace, "}, location{");
        LOG(kTrace, aLocation);
        LOG(kTrace, "}}\n");
        return !iRefreshing;
    }
    iLock.Signal();
    return false;
}

void CpiDeviceListUpnp::Start()
{
    iActive = true;
    iLock.Wait();
    TBool needsStart = !iStarted;
    iStarted = true;
    iLock.Signal();
    if (needsStart) {
        iSsdpLock.Wait();
        if (iUnicastListener != NULL) {
            iUnicastListener->Start();
        }
        iSsdpLock.Signal();
    }
}

void CpiDeviceListUpnp::Refresh()
{
    if (StartRefresh()) {
        return;
    }
    Start();
    TUint delayMs = Stack::InitParams().MsearchTimeSecs() * 1000;
    delayMs += 100; /* allow slightly longer to cope with devices which send
                       out Alive messages at the last possible moment */
    iRefreshTimer->FireIn(delayMs);
    /*  during refresh...
            on every Add():
                add to iRefreshMap
                check device against iMap, adding it and reporting as new if necessary
        on completion...
            on timer expiry:
                check each item in iMap
                    if it appears in iRefreshMap do nowt
                    else remove it from iMap and report this to observer */
}

TBool CpiDeviceListUpnp::IsDeviceReady(CpiDevice& aDevice)
{
    reinterpret_cast<CpiDeviceUpnp*>(aDevice.OwnerData())->FetchXml();
    return false;
}

TBool CpiDeviceListUpnp::IsLocationReachable(const Brx& aLocation) const
{
    /* linux's filtering of multicast messages appears to be buggy and messages
       received by all interfaces are sometimes delivered to sockets which are
       bound to / members of a group on a single (different) interface.  It'd be
       more correct to filter these out in SsdpListenerMulticast but that would
       require API changes which would be more inconvenient than just moving the
       filtering here.
       This should be reconsidered if we ever add more clients of SsdpListenerMulticast */
    TBool reachable = false;
    Uri uri;
    try {
        uri.Replace(aLocation);
    }
    catch (UriError&) {
        return false;
    }
    iLock.Wait();
    Endpoint endpt(0, uri.Host());
    NetworkAdapter* nif = Stack::NetworkAdapterList().CurrentAdapter("CpiDeviceListUpnp::IsLocationReachable");
    if (nif != NULL) {
        if (nif->Address() == iInterface && nif->ContainsAddress(endpt.Address())) {
            reachable = true;
        }
        nif->RemoveRef("CpiDeviceListUpnp::IsLocationReachable");
    }
    iLock.Signal();
    return reachable;
}

void CpiDeviceListUpnp::RefreshTimerComplete()
{
    RefreshComplete();
    Stack::Mutex().Wait();
    if (iPendingRefreshCount > 0) {
        iNextRefreshTimer->FireIn(Stack::InitParams().MsearchTimeSecs() * 1000);
        iPendingRefreshCount--;
    }
    Stack::Mutex().Signal();
}

void CpiDeviceListUpnp::NextRefreshDue()
{
    Refresh();
}

void CpiDeviceListUpnp::CurrentNetworkAdapterChanged()
{
    HandleInterfaceChange(false);
}

void CpiDeviceListUpnp::SubnetListChanged()
{
    HandleInterfaceChange(true);
}

void CpiDeviceListUpnp::HandleInterfaceChange(TBool aNewSubnet)
{
    NetworkAdapter* current = Stack::NetworkAdapterList().CurrentAdapter("CpiDeviceListUpnp::HandleInterfaceChange");
    if (aNewSubnet && current != NULL && current->Address() == iInterface) {
        // list of subnets has changed but our interface is still available so there's nothing for us to do here
        current->RemoveRef("CpiDeviceListUpnp::HandleInterfaceChange");
        return;
    }
    iNextRefreshTimer->Cancel();
    Stack::Mutex().Wait();
    iPendingRefreshCount = 0;
    Stack::Mutex().Signal();
    StopListeners();

    if (current == NULL) {
        iLock.Wait();
        iInterface = 0;
        iLock.Signal();
        RemoveAll();
        return;
    }

    // we used to only remove devices for subnet changes
    // its not clear why this was correct - any interface change will result in control/event urls changing
    RemoveAll();
    
    iLock.Wait();
    iInterface = current->Address();
    iLock.Signal();
    TUint msearchTime = Stack::InitParams().MsearchTimeSecs();
    Stack::Mutex().Wait();
    iPendingRefreshCount = (kMaxMsearchRetryForNewAdapterSecs + msearchTime - 1) / (2 * msearchTime);
    Stack::Mutex().Signal();
    current->RemoveRef("CpiDeviceListUpnp::HandleInterfaceChange");

    iSsdpLock.Wait();
    iUnicastListener = new SsdpListenerUnicast(*this, iInterface);
    iUnicastListener->Start();
    iMulticastListener = &Stack::MulticastListenerClaim(iInterface);
    iNotifyHandlerId = iMulticastListener->AddNotifyHandler(this);
    iSsdpLock.Signal();
    Refresh();
}

void CpiDeviceListUpnp::RemoveAll()
{
    iRefreshTimer->Cancel();
    iNextRefreshTimer->Cancel();
    iLock.Wait();
    CancelRefresh();
    Stack::Mutex().Wait();
    iPendingRefreshCount = 0;
    Stack::Mutex().Signal();
    std::vector<CpiDevice*> devices;
    Map::iterator it = iMap.begin();
    while (it != iMap.end()) {
        devices.push_back(it->second);
        it->second->AddRef();
        it++;
    }
    iLock.Signal();
    for (TUint i=0; i<(TUint)devices.size(); i++) {
        Remove(devices[i]->Udn());
        devices[i]->RemoveRef();
    }
}

void CpiDeviceListUpnp::XmlFetchCompleted(CpiDeviceUpnp& aDevice, TBool aError)
{
    if (aError) {
        LOG(kTrace, "Device xml fetch error {udn{");
        LOG(kTrace, aDevice.Udn());
        LOG(kTrace, "}, location{");
        LOG(kTrace, aDevice.Location());
        LOG(kTrace, "}}\n");
        Remove(aDevice.Udn());
    }
    else {
        SetDeviceReady(aDevice.Device());
    }
}

void CpiDeviceListUpnp::SsdpNotifyRootAlive(const Brx& aUuid, const Brx& aLocation, TUint aMaxAge)
{
    (void)Update(aUuid, aLocation, aMaxAge);
}

void CpiDeviceListUpnp::SsdpNotifyUuidAlive(const Brx& aUuid, const Brx& aLocation, TUint aMaxAge)
{
    (void)Update(aUuid, aLocation, aMaxAge);
}

void CpiDeviceListUpnp::SsdpNotifyDeviceTypeAlive(const Brx& aUuid, const Brx& /*aDomain*/, const Brx& /*aType*/,
                                                 TUint /*aVersion*/, const Brx& aLocation, TUint aMaxAge)
{
    (void)Update(aUuid, aLocation, aMaxAge);
}

void CpiDeviceListUpnp::SsdpNotifyServiceTypeAlive(const Brx& aUuid, const Brx& /*aDomain*/, const Brx& /*aType*/,
                                                  TUint /*aVersion*/, const Brx& aLocation, TUint aMaxAge)
{
    (void)Update(aUuid, aLocation, aMaxAge);
}

void CpiDeviceListUpnp::SsdpNotifyRootByeBye(const Brx& aUuid)
{
    Remove(aUuid);
}

void CpiDeviceListUpnp::SsdpNotifyUuidByeBye(const Brx& aUuid)
{
    Remove(aUuid);
}

void CpiDeviceListUpnp::SsdpNotifyDeviceTypeByeBye(const Brx& aUuid, const Brx& /*aDomain*/, const Brx& /*aType*/, TUint /*aVersion*/)
{
    Remove(aUuid);
}

void CpiDeviceListUpnp::SsdpNotifyServiceTypeByeBye(const Brx& aUuid, const Brx& /*aDomain*/, const Brx& /*aType*/, TUint /*aVersion*/)
{
    Remove(aUuid);
}


// CpiDeviceListUpnpAll

CpiDeviceListUpnpAll::CpiDeviceListUpnpAll(FunctorCpiDevice aAdded, FunctorCpiDevice aRemoved)
    : CpiDeviceListUpnp(aAdded, aRemoved)
{
}

CpiDeviceListUpnpAll::~CpiDeviceListUpnpAll()
{
    StopListeners();
}

void CpiDeviceListUpnpAll::Start()
{
    CpiDeviceListUpnp::Start();
    iSsdpLock.Wait();
    if (iUnicastListener != NULL) {
        try {
            iUnicastListener->MsearchAll();
        }
        catch (NetworkError&) {
            LOG2(kDevice, kError, "Error sending msearch for CpiDeviceListUpnpAll\n");
        }
        catch (WriterError&) {
            LOG2(kDevice, kError, "Error sending msearch for CpiDeviceListUpnpAll\n");
        }
    }
    iSsdpLock.Signal();
}

void CpiDeviceListUpnpAll::SsdpNotifyRootAlive(const Brx& aUuid, const Brx& aLocation, TUint aMaxAge)
{
    if (Update(aUuid, aLocation, aMaxAge)) {
        return;
    }
    if (IsLocationReachable(aLocation)) {
        CpiDeviceUpnp* device = new CpiDeviceUpnp(aUuid, aLocation, aMaxAge, *this, *this);
        Add(&device->Device());
    }
}


// CpiDeviceListUpnpRoot

CpiDeviceListUpnpRoot::CpiDeviceListUpnpRoot(FunctorCpiDevice aAdded, FunctorCpiDevice aRemoved)
    : CpiDeviceListUpnp(aAdded, aRemoved)
{
}

CpiDeviceListUpnpRoot::~CpiDeviceListUpnpRoot()
{
    StopListeners();
}

void CpiDeviceListUpnpRoot::Start()
{
    CpiDeviceListUpnp::Start();
    iSsdpLock.Wait();
    if (iUnicastListener != NULL) {
        try {
            iUnicastListener->MsearchRoot();
        }
        catch (NetworkError&) {
            LOG2(kDevice, kError, "Error sending msearch for CpiDeviceListUpnpRoot\n");
        }
        catch (WriterError&) {
            LOG2(kDevice, kError, "Error sending msearch for CpiDeviceListUpnpRoot\n");
        }
    }
    iSsdpLock.Signal();
}

void CpiDeviceListUpnpRoot::SsdpNotifyRootAlive(const Brx& aUuid, const Brx& aLocation, TUint aMaxAge)
{
    if (Update(aUuid, aLocation, aMaxAge)) {
        return;
    }
    if (IsLocationReachable(aLocation)) {
        CpiDeviceUpnp* device = new CpiDeviceUpnp(aUuid, aLocation, aMaxAge, *this, *this);
        Add(&device->Device());
    }
}


// CpiDeviceListUpnpUuid

CpiDeviceListUpnpUuid::CpiDeviceListUpnpUuid(const Brx& aUuid, FunctorCpiDevice aAdded, FunctorCpiDevice aRemoved)
    : CpiDeviceListUpnp(aAdded, aRemoved)
    , iUuid(aUuid)
{
}

CpiDeviceListUpnpUuid::~CpiDeviceListUpnpUuid()
{
    StopListeners();
}

void CpiDeviceListUpnpUuid::Start()
{
    CpiDeviceListUpnp::Start();
    iSsdpLock.Wait();
    if (iUnicastListener != NULL) {
        try {
            iUnicastListener->MsearchUuid(iUuid);
        }
        catch (NetworkError&) {
            LOG2(kDevice, kError, "Error sending msearch for CpiDeviceListUpnpUuid\n");
        }
        catch (WriterError&) {
            LOG2(kDevice, kError, "Error sending msearch for CpiDeviceListUpnpUuid\n");
        }
    }
    iSsdpLock.Signal();
}

void CpiDeviceListUpnpUuid::SsdpNotifyUuidAlive(const Brx& aUuid, const Brx& aLocation, TUint aMaxAge)
{
    if (aUuid != iUuid) {
        return;
    }
    if (Update(aUuid, aLocation, aMaxAge)) {
        return;
    }
    if (IsLocationReachable(aLocation)) {
        CpiDeviceUpnp* device = new CpiDeviceUpnp(aUuid, aLocation, aMaxAge, *this, *this);
        Add(&device->Device());
    }
}


// CpiDeviceListUpnpDeviceType

CpiDeviceListUpnpDeviceType::CpiDeviceListUpnpDeviceType(const Brx& aDomainName, const Brx& aDeviceType, TUint aVersion,
                                                         FunctorCpiDevice aAdded, FunctorCpiDevice aRemoved)
    : CpiDeviceListUpnp(aAdded, aRemoved)
    , iDomainName(aDomainName)
    , iDeviceType(aDeviceType)
    , iVersion(aVersion)
{
}

CpiDeviceListUpnpDeviceType::~CpiDeviceListUpnpDeviceType()
{
    StopListeners();
}

void CpiDeviceListUpnpDeviceType::Start()
{
    CpiDeviceListUpnp::Start();
    iSsdpLock.Wait();
    if (iUnicastListener != NULL) {
        try {
            iUnicastListener->MsearchDeviceType(iDomainName, iDeviceType, iVersion);
        }
        catch (NetworkError&) {
            LOG2(kDevice, kError, "Error sending msearch for CpiDeviceListUpnpDeviceType\n");
        }
        catch (WriterError&) {
            LOG2(kDevice, kError, "Error sending msearch for CpiDeviceListUpnpDeviceType\n");
        }
    }
    iSsdpLock.Signal();
}

void CpiDeviceListUpnpDeviceType::SsdpNotifyDeviceTypeAlive(const Brx& aUuid, const Brx& aDomain, const Brx& aType,
                                                            TUint aVersion, const Brx& aLocation, TUint aMaxAge)
{
    if (aVersion<iVersion || aDomain!=iDomainName || aType!=iDeviceType) {
        return;
    }
    if (Update(aUuid, aLocation, aMaxAge)) {
        return;
    }
    if (IsLocationReachable(aLocation)) {
        CpiDeviceUpnp* device = new CpiDeviceUpnp(aUuid, aLocation, aMaxAge, *this, *this);
        Add(&device->Device());
    }
}


// CpiDeviceListUpnpServiceType

CpiDeviceListUpnpServiceType::CpiDeviceListUpnpServiceType(const Brx& aDomainName, const Brx& aServiceType, TUint aVersion,
                                                           FunctorCpiDevice aAdded, FunctorCpiDevice aRemoved)
    : CpiDeviceListUpnp(aAdded, aRemoved)
    , iDomainName(aDomainName)
    , iServiceType(aServiceType)
    , iVersion(aVersion)
{
}

CpiDeviceListUpnpServiceType::~CpiDeviceListUpnpServiceType()
{
    StopListeners();
}

void CpiDeviceListUpnpServiceType::Start()
{
    CpiDeviceListUpnp::Start();
    iSsdpLock.Wait();
    if (iUnicastListener != NULL) {
        try {
            iUnicastListener->MsearchServiceType(iDomainName, iServiceType, iVersion);
        }
        catch (NetworkError&) {
            LOG2(kDevice, kError, "Error sending msearch for CpiDeviceListUpnpServiceType\n");
        }
        catch (WriterError&) {
            LOG2(kDevice, kError, "Error sending msearch for CpiDeviceListUpnpServiceType\n");
        }
    }
    iSsdpLock.Signal();
}

void CpiDeviceListUpnpServiceType::SsdpNotifyServiceTypeAlive(const Brx& aUuid, const Brx& aDomain, const Brx& aType,
                                                              TUint aVersion, const Brx& aLocation, TUint aMaxAge)
{
    if (aVersion<iVersion || aDomain!=iDomainName || aType!=iServiceType) {
        return;
    }
    if (Update(aUuid, aLocation, aMaxAge)) {
        return;
    }
    if (IsLocationReachable(aLocation)) {
        CpiDeviceUpnp* device = new CpiDeviceUpnp(aUuid, aLocation, aMaxAge, *this, *this);
        Add(&device->Device());
    }
}
