from openai import OpenAI
import requests

def supports_responses_api(base_url: str) -> bool:
    """
    探测是否支持 /v1/responses
    """
    try:
        url = base_url.rstrip("/") + "/responses"
        # 用 HEAD 或小测试
        r = requests.post(url, json={"model": "test", "input": "ping"}, timeout=3)
        return r.status_code != 404  # 404 = 不支持
    except Exception:
        return False


def create_client(api_key: str, base_url: str):
    return OpenAI(api_key=api_key, base_url=base_url)


def ask(client, base_url: str, model: str, prompt: str):
    """
    自动适配 responses.create 和 chat.completions.create
    """
    if supports_responses_api(base_url):
        # -------------------------------
        # 新接口 responses.create()
        # -------------------------------
        response = client.responses.create(
            model=model,
            input=prompt
        )
        return response.output_text
    else:
        # -------------------------------
        # 旧接口 chat.completions.create()
        # -------------------------------
        response = client.chat.completions.create(
            model=model,
            messages=[{"role": "user", "content": prompt}],
        )
        return response.choices[0].message["content"]


# -------------------------------
# 使用示例
# -------------------------------

api_key = "sk-cmbKb2uwpmEpNLoUzox7c3bWae8IclRFofXj32nyPOTGm7ur"
base_url = "https://api.chatanywhere.tech/v1"  # 或任意第三方 / 官方

client = create_client(api_key, base_url)

reply = ask(client, base_url, "gpt-4o-mini", "请总结一下强化学习是什么？")

print("\nAI 回答：\n", reply)
