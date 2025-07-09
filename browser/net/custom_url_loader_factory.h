#ifndef CHROME_BROWSER_NET_CUSTOM_URL_LOADER_FACTORY_H_
#define CHROME_BROWSER_NET_CUSTOM_URL_LOADER_FACTORY_H_

#include "services/network/public/mojom/url_loader_factory.mojom.h"

class CustomURLLoaderFactory : public network::mojom::URLLoaderFactory {
 public:
  explicit CustomURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory);

  ~CustomURLLoaderFactory() override;

  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;

 private:
  void OnConnectionError();

  mojo::Remote<network::mojom::URLLoaderFactory> target_factory_;
  mojo::Receiver<network::mojom::URLLoaderFactory> receiver_;
};

#endif  // CHROME_BROWSER_NET_CUSTOM_URL_LOADER_FACTORY_H_
