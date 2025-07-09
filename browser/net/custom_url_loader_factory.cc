#include "chrome/browser/net/custom_url_loader_factory.h"

#include "chrome/browser/net/custom_url_loader.h"

CustomURLLoaderFactory::CustomURLLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory)
    : target_factory_(std::move(target_factory)),
      receiver_(this, std::move(factory_receiver)) {
  receiver_.set_disconnect_handler(base::BindOnce(
      &CustomURLLoaderFactory::OnConnectionError, base::Unretained(this)));
}

void CustomURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  // Create our custom loader instead of the default one
  new CustomURLLoader(std::move(receiver), std::move(client), request);
}

void CustomURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  target_factory_->Clone(std::move(receiver));
}

void CustomURLLoaderFactory::OnConnectionError() {
  delete this;
}
