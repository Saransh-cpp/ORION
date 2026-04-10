import requests
import json
import os
import io
import torch
from PIL import Image
from transformers import AutoProcessor, AutoModelForImageTextToText

# --- CONFIGURATION ---
SIMSAT_STATIC_API = "http://localhost:9005/data/image/mapbox"
SAVE_DIR = "calibration_images"

# Our test targets (Target Name, Longitude, Latitude)
TEST_TARGETS = [
    ("New_York_City", -74.0060, 40.7128),
    ("Sahara_Desert", 11.2684, 24.3211),
    ("Atlantic_Ocean", -40.0000, 30.0000)
]

# --- SETUP HARDWARE ACCELERATION ---
if torch.cuda.is_available():
    device = torch.device("cuda")
    print("[⚙️] Hardware Accelerator: NVIDIA GPU (CUDA)")
elif torch.backends.mps.is_available():
    device = torch.device("mps")
    print("[⚙️] Hardware Accelerator: Apple Silicon GPU (Metal/MPS)")
else:
    device = torch.device("cpu")
    print("[⚙️] Hardware Accelerator: CPU (Expect slower inference)")

# --- LOAD LIQUID AI MODEL ---
print("[⚙️] Loading LiquidAI/LFM2.5-VL-1.6B...")
processor = AutoProcessor.from_pretrained("LiquidAI/LFM2.5-VL-1.6B", trust_remote_code=True)
model = AutoModelForImageTextToText.from_pretrained("LiquidAI/LFM2.5-VL-1.6B", trust_remote_code=True).to(device)
print("[✅] Model loaded successfully!")

def fetch_static_image(lon, lat):
    """Fetches an image directly above a specific coordinate."""
    print(f"\n[🛰️] Positioning satellite over {lon}, {lat}...")
    
    params = {
        "lon_target": lon,
        "lat_target": lat,
        "lon_satellite": lon,
        "lat_satellite": lat,
        "alt_satellite": 500.0
    }
    
    try:
        response = requests.get(SIMSAT_STATIC_API, params=params)
        response.raise_for_status()
        return response.content
    except Exception as e:
        print(f"[!] Error fetching static image: {e}")
        return None

def analyze_with_liquid_vlm(image_bytes):
    """Passes the image and prompt through the actual Liquid VLM."""
    print(f"[🧠] Analyzing image on {device}...")
    
    # 1. Convert raw bytes to a PIL Image
    image = Image.open(io.BytesIO(image_bytes)).convert("RGB")
    
    prompt = """You are an autonomous orbital triage assistant. Analyze this high-resolution RGB satellite image and classify its contents.
    
    Strictly use one of these categories:
    - HIGH: Contains anomalies, natural disasters, unusual landscapes, or potential illegal/unregulated human activity.
    - MEDIUM: Contains ordinary human infrastructure (cities, roads, farms, standard maritime traffic).
    - LOW: Contains featureless ocean, empty/barren wasteland, or rendering bugs.
    
    You MUST output your response as a valid JSON object with exactly two keys: "category" and "reason". Do not include any other text or markdown blocks.
    Example: {"category": "MEDIUM", "reason": "The image shows a standard urban grid with residential housing."}"""
    
    # 2. Format the conversational dictionary
    conversation = [
        {
            "role": "user",
            "content": [
                {"type": "image", "image": image},
                {"type": "text", "text": prompt},
            ],
        },
    ]
    
    # 3. Use the processor's chat template method (The Fix!)
    inputs = processor.apply_chat_template(
        conversation,
        add_generation_prompt=True,
        return_tensors="pt",
        return_dict=True,
        tokenize=True,
    ).to(device)
    
    # 4. Generate the response
    with torch.no_grad():
        # Liquid AI recommends slightly different params for their VLMs
        outputs = model.generate(
            **inputs, 
            max_new_tokens=150,
            temperature=0.1,
            repetition_penalty=1.05
        )
        
    # 5. Isolate only the newly generated tokens
    # inputs["input_ids"].shape[1] gives us the exact length of the prompt
    generated_ids = outputs[0, inputs["input_ids"].shape[1]:]
    
    # 6. Decode only the model's response
    full_text = processor.decode(generated_ids, skip_special_tokens=True)
    
    # VLM outputs often include the prompt text. Let's isolate the JSON block.
    start_idx = full_text.find('{')
    end_idx = full_text.rfind('}')
    
    if start_idx != -1 and end_idx != -1:
        return full_text[start_idx:end_idx+1]
    
    return full_text


def parse_vlm_response(raw_text):
    """Safely parses the JSON output from the VLM."""
    try:
        cleaned_text = raw_text.strip()
        data = json.loads(cleaned_text)
        return data.get("category", "LOW"), data.get("reason", "No reason provided.")
    except json.JSONDecodeError:
        print(f"[!] Failed to parse JSON. Raw output:\n{raw_text}")
        return "LOW", "Failed to parse JSON."

def main():
    print("=== VLM CALIBRATION SUITE ONLINE ===")
    
    if not os.path.exists(SAVE_DIR):
        os.makedirs(SAVE_DIR)
        
    for name, lon, lat in TEST_TARGETS:
        print(f"--- Target: {name} ---")
        
        img_bytes = fetch_static_image(lon, lat)
        if not img_bytes:
            continue
            
        print(f"[📸] Image fetched successfully! Size: {len(img_bytes)} bytes.")
        
        filename = os.path.join(SAVE_DIR, f"{name}.png")
        with open(filename, "wb") as f:
            f.write(img_bytes)
        print(f"[💾] Saved ground truth to: {filename}")
        
        raw_vlm_output = analyze_with_liquid_vlm(img_bytes)
        category, reason = parse_vlm_response(raw_vlm_output)
        
        print(f"[📊] VLM Verdict: {category} - {reason}\n")

if __name__ == "__main__":
    main()
