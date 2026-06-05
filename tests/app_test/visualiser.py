import numpy as np
import plotly.graph_objects as go
from scipy.io import wavfile

# Define the path to your 10s wav file
file_path = "10sNN.wav"

# 1. Read the audio data
sample_rate, data = wavfile.read(file_path)

# 2. Handle stereo data if necessary (extract the first channel)
if data.ndim > 1:
    data = data[:, 0]

# 3. Create a precise time vector in seconds
# This ensures your X-axis is in actual time rather than just sample indices
time = np.linspace(0, len(data) / sample_rate, num=len(data))

# 4. Initialize the interactive figure
fig = go.Figure()

fig.add_trace(go.Scatter(
    x=time,
    y=data,
    mode='lines',
    name='Signal Amplitude',
    line=dict(color='#00bc8c', width=1),
    # If the file has a massive sample rate, WebGL improves rendering performance
    hoverinfo='x+y' 
))

# 5. Format the layout for a clean, technical interface
fig.update_layout(
    title='Biosignal Time-Domain Visualization',
    xaxis_title='Time (s)',
    yaxis_title='Amplitude',
    template='plotly_dark',  # Dark mode interface
    xaxis=dict(
        rangeslider=dict(visible=True),  # Adds the minimap for zooming
        type="linear"
    ),
    yaxis=dict(
        fixedrange=False  # Allows vertical zooming as well
    )
)

# Open the interactive graph in your default web browser
fig.show()