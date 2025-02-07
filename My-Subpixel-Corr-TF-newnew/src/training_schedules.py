LONG_SCHEDULE = {
    'step_values': [400000, 600000, 800000, 1000000],
    'learning_rates': [0.0001, 0.00005, 0.000025, 0.0000125, 0.00000625],
    'momentum': 0.9,
    'momentum2': 0.999,
    'weight_decay': 0.0004,
    'max_iter': 1200000,
}

SUBPIXEL_SCHEDULE = {
    #'step_values': [8000, 11000, 14000, 17000],
    # 'step_values': [17000, 23000, 29000, 35000],
    'step_values': [40000, 55000, 70000, 85000],
    'learning_rates': [0.0001, 0.00005, 0.000025, 0.0000125, 0.00000625], # (1)
    #'learning_rates': [0.0005, 0.000025, 0.000125, 0.0000125, 0.00003125], # (2)
    'momentum': 0.9,
    'momentum2': 0.999,
    'weight_decay': 0.0004,
    'max_iter': 100000,
}

FINETUNE_SCHEDULE = {
    # TODO: Finetune schedule
}
