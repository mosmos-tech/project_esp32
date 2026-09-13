Rails.application.routes.draw do
  root "sensor_logs#index"
  resources :sensor_logs, only: [:index, :create]
end