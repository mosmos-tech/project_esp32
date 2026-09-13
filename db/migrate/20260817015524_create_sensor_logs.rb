class CreateSensorLogs < ActiveRecord::Migration[8.1]
  def change
    create_table :sensor_logs do |t|
      t.float :temperature
      t.float :humidity

      t.timestamps
    end
  end
end
