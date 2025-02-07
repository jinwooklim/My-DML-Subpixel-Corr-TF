from ..dataloader import load_batch
from ..dataset_configs import FLYING_CHAIRS_DATASET_CONFIG
from ..training_schedules import LONG_SCHEDULE
from ..training_schedules import SUBPIXEL_SCHEDULE
from .flownet_c import FlowNetC

# Create a new network
net = FlowNetC()

# Load a batch of data
input_a, input_b, flow = load_batch(FLYING_CHAIRS_DATASET_CONFIG, 'train', net.global_step)

# Train on the data
net.train(
    log_dir='./logs/flownet_c',
    #training_schedule=LONG_SCHEDULE,
    training_schedule=SUBPIXEL_SCHEDULE,
    input_a=input_a,
    input_b=input_b,
    flow=flow
)
