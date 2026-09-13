class SensorLogsController < ApplicationController
  skip_before_action :verify_authenticity_token, only: [:create] 

  def index
    # ดึงข้อมูล 15 รายการล่าสุด และสลับลำดับจากเก่าไปใหม่ (เพื่อให้กราฟวาดเส้นจากซ้ายไปขวา)
    @logs = SensorLog.order(created_at: :desc).limit(15).reverse
    
    # กำหนดให้ตอบสนองได้ทั้ง HTML (หน้าเว็บ) และ JSON (ส่งข้อมูลให้กราฟ)
    respond_to do |format|
      format.html
      format.json { render json: @logs }
    end
  end

  def create
    @log = SensorLog.new(sensor_log_params)
    if @log.save
      render json: { status: 'success', data: @log }, status: :created
    else
      render json: { status: 'error', errors: @log.errors }, status: :unprocessable_entity
    end
  end

  private

  def sensor_log_params
    params.require(:sensor_log).permit(:temperature, :humidity)
  end
end