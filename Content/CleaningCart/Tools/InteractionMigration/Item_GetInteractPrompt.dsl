(fn GetInteractPrompt (Interactor)
  (if (Variables|Default|GetItemCarrying)
    (return "놓기 / 원래 자리에 돌려놓기")
    (else (return "집기"))))
