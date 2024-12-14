import numpy as np
from PIL import Image
import os

def get_future_frame(decoded_frames, bin_files, input_dir, current_index, width, height, subsampling):
    """
    Retrieves the future frame for the current B-frame decoding.
    If the frame is not yet decoded, it decodes it from the .bin file.
    """
    # Find the future frame's index
    future_index = current_index + 1

    # If the future frame is already decoded, return it
    if future_index < len(decoded_frames):
        return decoded_frames[future_index]

    # If the future frame is not decoded, decode it
    if future_index < len(bin_files):
        future_bin_file = bin_files[future_index]
        future_file_path = os.path.join(input_dir, future_bin_file)
        return decode_yuv(future_file_path, width, height, subsampling)
    
    # If no future frame exists (e.g., last frame), return a blank frame
    return np.zeros((height, width), dtype=np.uint8)

def yuv_to_rgb(y, u, v):
    """
    Convert YUV to RGB.
    """
    y = y.astype(float)
    u = u.astype(float) - 128
    v = v.astype(float) - 128

    r = np.clip(y + 1.402 * v, 0, 255)
    g = np.clip(y - 0.344136 * u - 0.714136 * v, 0, 255)
    b = np.clip(y + 1.772 * u, 0, 255)

    return np.stack((r, g, b), axis=-1).astype(np.uint8)

def decode_gop(input_dir, output_dir, width, height, gop_size, subsampling='4:2:0'):
    """
    Decode a GOP from .bin files and reconstruct frames.
    """
    bin_files = sorted([f for f in os.listdir(input_dir) if f.endswith('.bin')])
    os.makedirs(output_dir, exist_ok=True)

    decoded_frames = []  # To store reconstructed frames

    for i, bin_file in enumerate(bin_files):
        file_path = os.path.join(input_dir, bin_file)
        frame_type = determine_frame_type(i, gop_size)

        print(f"Decoding {bin_file} as {frame_type}...")

        if frame_type == "I_FRAME":
            # Directly decode the I-frame
            frame = decode_yuv(file_path, width, height, subsampling)
        elif frame_type == "P_FRAME":
            # Predict using the previous frame and motion vectors
            previous_frame = decoded_frames[-1]
            frame = decode_p_frame(file_path, previous_frame, width, height)
        elif frame_type == "B_FRAME":
            # Bidirectionally predict using previous and future frames
            previous_frame = decoded_frames[-1]
            future_frame = get_future_frame(decoded_frames, bin_files, input_dir, i, width, height, subsampling)
            frame = decode_b_frame(file_path, previous_frame, future_frame, width, height)

        decoded_frames.append(frame)

        # Save the frame as an image
        output_path = os.path.join(output_dir, f"frame_{i:03d}.png")
        Image.fromarray(frame).save(output_path)
        print(f"Saved {output_path}")


def determine_frame_type(index, gop_size):
    """
    Determine the frame type based on index and GOP size.
    """
    if index % gop_size == 0:
        return "I_FRAME"
    elif (index % gop_size - 1) % 3 == 0:
        return "P_FRAME"
    else:
        return "B_FRAME"


def decode_yuv(file_path, width, height, subsampling):
    """
    Decode a YUV frame from a .bin file.
    """
    with open(file_path, 'rb') as f:
        y_size = width * height

        if subsampling == '4:4:4':
            u_size = v_size = y_size
        elif subsampling == '4:2:2':
            u_size = v_size = y_size // 2
        elif subsampling == '4:2:0':
            u_size = v_size = y_size // 4
        else:
            raise ValueError("Unsupported subsampling format.")

        y = np.frombuffer(f.read(y_size), dtype=np.uint8).reshape((height, width))
        u = np.frombuffer(f.read(u_size), dtype=np.uint8).reshape((height // 2, width // 2))
        v = np.frombuffer(f.read(v_size), dtype=np.uint8).reshape((height // 2, width // 2))

        # Upsample U and V channels
        u = u.repeat(2, axis=0).repeat(2, axis=1)[:height, :width]
        v = v.repeat(2, axis=0).repeat(2, axis=1)[:height, :width]

        return yuv_to_rgb(y, u, v)


def decode_p_frame(file_path, previous_frame, width, height):
    """
    Decode a P-frame using the previous frame and motion vectors.
    """
    # Load motion vectors and residuals
    motion_vectors, residuals = load_p_frame_data(file_path, width, height)

    # Predict the frame
    predicted_frame = motion_compensation(previous_frame, motion_vectors, width, height)

    # Add residuals to reconstruct the current frame
    return np.clip(predicted_frame + residuals, 0, 255).astype(np.uint8)


def decode_b_frame(file_path, previous_frame, future_frame, width, height):
    """
    Decode a B-frame using previous and future frames.
    """
    # Load motion vectors and residuals
    forward_motion_vectors, backward_motion_vectors, residuals = load_b_frame_data(file_path, width, height)

    # Predict the frame bidirectionally
    forward_predicted = motion_compensation(previous_frame, forward_motion_vectors, width, height)
    backward_predicted = motion_compensation(future_frame, backward_motion_vectors, width, height)

    # Combine predictions
    bidirectional_prediction = (forward_predicted + backward_predicted) // 2

    # Add residuals to reconstruct the current frame
    return np.clip(bidirectional_prediction + residuals, 0, 255).astype(np.uint8)


def motion_compensation(reference_frame, motion_vectors, width, height):
    """
    Perform motion compensation given reference frame and motion vectors.
    """
    block_size = 16
    compensated_frame = np.zeros_like(reference_frame)

    for block_y in range(0, height, block_size):
        for block_x in range(0, width, block_size):
            block_index = (block_y // block_size) * (width // block_size) + (block_x // block_size)
            mv = motion_vectors[block_index]

            ref_y = block_y + mv.dy
            ref_x = block_x + mv.dx

            compensated_frame[block_y:block_y + block_size, block_x:block_x + block_size] = \
                reference_frame[ref_y:ref_y + block_size, ref_x:ref_x + block_size]

    return compensated_frame


def load_p_frame_data(file_path, width, height):
    """
    Load motion vectors and residuals for a P-frame from a .bin file.
    """
    with open(file_path, 'rb') as f:
        num_blocks = (width // 16) * (height // 16)
        motion_vectors = np.frombuffer(f.read(num_blocks * 4), dtype=np.int16).reshape((num_blocks, 2))
        residuals = np.frombuffer(f.read(width * height), dtype=np.int16).reshape((height, width))
    return motion_vectors, residuals


def load_b_frame_data(file_path, width, height):
    """
    Load motion vectors and residuals for a B-frame from a .bin file.
    """
    with open(file_path, 'rb') as f:
        num_blocks = (width // 16) * (height // 16)
        forward_motion_vectors = np.frombuffer(f.read(num_blocks * 4), dtype=np.int16).reshape((num_blocks, 2))
        backward_motion_vectors = np.frombuffer(f.read(num_blocks * 4), dtype=np.int16).reshape((num_blocks, 2))
        residuals = np.frombuffer(f.read(width * height), dtype=np.int16).reshape((height, width))
    return forward_motion_vectors, backward_motion_vectors, residuals


# Example Usage
input_directory = '/home/wsoxl/Encoder/MPEG_encoder/binoutput'  # Directory containing .bin files
output_directory = './decoded_frames'  # Directory to save frames
width, height = 3840, 2160
gop_size = 8  # Frames per GOP

decode_gop(input_directory, output_directory, width, height, gop_size)